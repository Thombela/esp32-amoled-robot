#include "wifi_poll.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <esp_eap_client.h>

#define NVS_NAMESPACE "clawd_wifi"
#define POLL_INTERVAL_MS 60000
#define WAIT_STEP_MS 500

static char cfg_ssid[33]   = {0};
static char cfg_pass[65]   = {0};
static char cfg_user[65]   = {0};
static bool cfg_enterprise = false;
static bool cfg_use_ttls   = false;
static char cfg_url[160]   = {0};
static char cfg_secret[65] = {0};

static char rx_buf[1024];
static volatile bool data_ready = false;
static volatile int last_http_code = 0;
static volatile bool connect_requested = false;

// WiFi.begin() is fire-and-forget — without this, a failed connect (wrong
// password, EAP rejected, AP out of range, etc.) only ever surfaces as a
// generic "Connection failed" on the credential screen once the 15s UI
// timeout expires, with no way to tell WHY. This logs the ESP-IDF-level
// disconnect reason code (auth failure, handshake timeout, no AP found,
// EAP rejection, ...) so a real cause shows up in Serial instead of having
// to guess blind — especially useful for enterprise/802.1x, where several
// different things can go wrong (wrong EAP method, bad identity, cert
// issues) that all look identical from the credential screen alone.
static void on_wifi_event(WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Serial.println("WiFi event: associated with AP");
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.printf("WiFi event: got IP %s\n", WiFi.localIP().toString().c_str());
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
            uint8_t reason = info.wifi_sta_disconnected.reason;
            Serial.printf("WiFi event: disconnected, reason=%u (%s)\n",
                reason, WiFi.STA.disconnectReasonName((wifi_err_reason_t)reason));
            break;
        }
        default:
            break;
    }
}

static void load_config(void) {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);
    prefs.getString("ssid", cfg_ssid, sizeof(cfg_ssid));
    prefs.getString("pass", cfg_pass, sizeof(cfg_pass));
    prefs.getString("user", cfg_user, sizeof(cfg_user));
    cfg_enterprise = prefs.getUChar("ent", 0) != 0;
    cfg_use_ttls = prefs.getUChar("ttls", 0) != 0;
    prefs.getString("url", cfg_url, sizeof(cfg_url));
    prefs.getString("secret", cfg_secret, sizeof(cfg_secret));
    prefs.end();
}

// Only ever called from wifi_poll_task, never directly from a caller's own
// thread — WiFi.disconnect()/WiFi.begin() (especially the enterprise/802.1x
// path, which can block for seconds negotiating EAP against a slow or
// unreachable RADIUS server) used to run synchronously inside whichever
// thread called wifi_poll_connect()/wifi_poll_set_ssid() — the same loop()
// thread that also drives LVGL rendering and touch handling. That froze the
// whole UI for the duration (and risked a watchdog reset if it blocked long
// enough). Now every caller just sets connect_requested and this background
// task (already running for the usage-data poll below) picks it up within
// WAIT_STEP_MS, off the UI thread entirely.
static void apply_connection(void) {
    if (!cfg_ssid[0]) return;
    // WiFiSTAClass::disconnect(wifioff, eraseap, ...) — NOT (eraseap) as the
    // single bool arg might suggest. disconnect(true) alone turns the radio
    // off (wifioff=true) rather than clearing old AP config, which left the
    // driver in a bad state across connect attempts (symptom: an enterprise
    // connect using correct credentials still failed, and a subsequent scan
    // then reported "Scan failed"). A full off/erase/restart cycle before
    // every connect attempt is also the standard ESP-IDF recommendation when
    // switching in or out of enterprise (802.1x) mode, since stale EAP state
    // doesn't always get cleared by a plain reconnect.
    WiFi.disconnect(true, true);
    delay(100);
    WiFi.mode(WIFI_STA);
    // The IDF WiFi driver's default auto-reconnect retries a failed connect
    // almost continuously (observed: dozens of attempts/second against a
    // rejecting RADIUS server) — bad for the AP/RADIUS side regardless of
    // whose fault the rejection is, and could plausibly trip an enterprise
    // network's own IDS/rate-limiting. We already have our own retry: the
    // user taps Connect again (or Refresh), so the driver doesn't need to.
    WiFi.setAutoReconnect(false);
    if (cfg_enterprise) {
        // Identity == username (no separate anonymous-identity field).
        // PEAP vs TTLS is a user-flippable toggle on the credential screen
        // (see wifi_poll_connect()) rather than assumed — both are common,
        // and a 802_1X_AUTH_FAILED from the wrong one looks identical to a
        // wrong password from the credential screen alone.
        wpa2_auth_method_t method = cfg_use_ttls ? WPA2_AUTH_TTLS : WPA2_AUTH_PEAP;
        // Never log the actual credentials — lengths only, to rule out a
        // keyboard-input bug (truncation, stray whitespace) as the cause of
        // a persistent 802_1X_AUTH_FAILED against verified-good credentials.
        Serial.printf("WiFi enterprise connect: method=%s user_len=%d pass_len=%d\n",
            cfg_use_ttls ? "TTLS" : "PEAP", strlen(cfg_user), strlen(cfg_pass));
        // Explicit inner-auth method rather than leaving it on ESP-IDF's
        // default (-1) — despite the "ttls_phase2" name, arduino-esp32's
        // STA.cpp applies this to PEAP too (it's set before the PEAP/TTLS
        // branch, unconditionally). MSCHAPv2 is the near-universal inner
        // method for enterprise/domain accounts; if the previous default
        // didn't match what this RADIUS server expects, that alone could
        // produce an identical 802_1X_AUTH_FAILED under both PEAP and TTLS.
        WiFi.begin(cfg_ssid, method, cfg_user, cfg_user, cfg_pass,
            nullptr, nullptr, nullptr, ESP_EAP_TTLS_PHASE2_MSCHAPV2);
    } else {
        WiFi.begin(cfg_ssid, cfg_pass);
    }
}

static void wifi_poll_task(void*) {
    for (;;) {
        if (connect_requested) {
            connect_requested = false;
            apply_connection();
        }

        if (WiFi.status() == WL_CONNECTED && cfg_url[0]) {
            WiFiClientSecure client;
            // v1 trade-off: skips server cert validation, so a MITM on the path
            // between this board and the tunnel edge could see the X-Clawd-Key
            // header. Acceptable for a personal project on home WiFi; flagged
            // here deliberately rather than silently assumed.
            client.setInsecure();
            HTTPClient http;
            http.setTimeout(8000);
            if (http.begin(client, cfg_url)) {
                if (cfg_secret[0]) http.addHeader("X-Clawd-Key", cfg_secret);
                int code = http.GET();
                last_http_code = code;
                if (code == 200) {
                    String body = http.getString();
                    size_t len = min(body.length(), sizeof(rx_buf) - 1);
                    memcpy(rx_buf, body.c_str(), len);
                    rx_buf[len] = '\0';
                    data_ready = true;
                }
                Serial.printf("WiFi poll: HTTP %d\n", code);
                http.end();
            }
        }
        // Fixed 60s cadence, matching the daemon's POLL_INTERVAL, but waited
        // out in short steps so a fresh connect_requested (or a serial
        // command) doesn't have to sit through the rest of the cycle.
        for (int waited = 0; waited < POLL_INTERVAL_MS && !connect_requested; waited += WAIT_STEP_MS) {
            vTaskDelay(pdMS_TO_TICKS(WAIT_STEP_MS));
        }
    }
}

void wifi_poll_init(void) {
    load_config();
    WiFi.onEvent(on_wifi_event);
    // The poll task is always created (it's a no-op until cfg_url is set) so that
    // configuring WiFi for the first time via the wifi_ssid/.../wifi_url serial
    // commands takes effect immediately, with no reboot required.
    xTaskCreatePinnedToCore(wifi_poll_task, "wifi_poll", 8192, nullptr, 1, nullptr, 0);
    if (cfg_ssid[0]) {
        connect_requested = true;
        Serial.printf("WiFi poll: connecting to '%s'...\n", cfg_ssid);
    } else {
        Serial.println("WiFi poll: no SSID configured yet (BLE-only mode)");
    }
}

bool wifi_poll_has_data(void) {
    return data_ready;
}

const char* wifi_poll_get_data(void) {
    data_ready = false;
    return rx_buf;
}

void wifi_poll_set_ssid(const char* v) {
    strlcpy(cfg_ssid, v, sizeof(cfg_ssid));
    cfg_enterprise = false;
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putString("ssid", cfg_ssid);
    prefs.putUChar("ent", 0);
    prefs.end();
    connect_requested = true;
    Serial.printf("WiFi ssid set to '%s'\n", cfg_ssid);
}

void wifi_poll_set_pass(const char* v) {
    strlcpy(cfg_pass, v, sizeof(cfg_pass));
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putString("pass", cfg_pass);
    prefs.end();
    if (cfg_ssid[0]) connect_requested = true;
    Serial.println("WiFi password set");
}

void wifi_poll_set_url(const char* v) {
    strlcpy(cfg_url, v, sizeof(cfg_url));
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putString("url", cfg_url);
    prefs.end();
    Serial.printf("WiFi poll url set to '%s'\n", cfg_url);
}

void wifi_poll_set_secret(const char* v) {
    strlcpy(cfg_secret, v, sizeof(cfg_secret));
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putString("secret", cfg_secret);
    prefs.end();
    Serial.println("WiFi poll secret set");
}

void wifi_poll_connect(const char* ssid, const char* username, const char* password,
                       bool enterprise, bool use_ttls) {
    strlcpy(cfg_ssid, ssid, sizeof(cfg_ssid));
    strlcpy(cfg_pass, password ? password : "", sizeof(cfg_pass));
    strlcpy(cfg_user, enterprise && username ? username : "", sizeof(cfg_user));
    cfg_enterprise = enterprise;
    cfg_use_ttls = enterprise && use_ttls;

    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putString("ssid", cfg_ssid);
    prefs.putString("pass", cfg_pass);
    prefs.putString("user", cfg_user);
    prefs.putUChar("ent", cfg_enterprise ? 1 : 0);
    prefs.putUChar("ttls", cfg_use_ttls ? 1 : 0);
    prefs.end();

    connect_requested = true;
    // Never log the password; SSID/mode only.
    Serial.printf("WiFi connect requested: ssid='%s' enterprise=%d ttls=%d\n",
        cfg_ssid, cfg_enterprise, cfg_use_ttls);
}

bool wifi_poll_is_connected(void) {
    return WiFi.status() == WL_CONNECTED;
}

bool wifi_poll_is_enterprise(void) {
    return cfg_enterprise;
}

bool wifi_poll_uses_ttls(void) {
    return cfg_use_ttls;
}

const char* wifi_poll_get_ssid(void) {
    return cfg_ssid;
}

const char* wifi_poll_get_username(void) {
    return cfg_user;
}

const char* wifi_poll_get_ip(void) {
    static char ip_buf[16];  // "255.255.255.255\0"
    if (WiFi.status() == WL_CONNECTED) {
        strlcpy(ip_buf, WiFi.localIP().toString().c_str(), sizeof(ip_buf));
    } else {
        ip_buf[0] = '\0';
    }
    return ip_buf;
}

void wifi_poll_print_status(void) {
    Serial.printf("WiFi status: ssid='%s' enterprise=%d connected=%d ip=%s url='%s' last_http=%d\n",
        cfg_ssid, cfg_enterprise, WiFi.status() == WL_CONNECTED, WiFi.localIP().toString().c_str(),
        cfg_url, last_http_code);
}
