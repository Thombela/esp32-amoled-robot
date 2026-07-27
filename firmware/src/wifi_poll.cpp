#include "wifi_poll.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>

#define NVS_NAMESPACE "clawd_wifi"

static char cfg_ssid[33]   = {0};
static char cfg_pass[65]   = {0};
static char cfg_url[160]   = {0};
static char cfg_secret[65] = {0};

static char rx_buf[1024];
static volatile bool data_ready = false;
static volatile int last_http_code = 0;

static void load_config(void) {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);
    prefs.getString("ssid", cfg_ssid, sizeof(cfg_ssid));
    prefs.getString("pass", cfg_pass, sizeof(cfg_pass));
    prefs.getString("url", cfg_url, sizeof(cfg_url));
    prefs.getString("secret", cfg_secret, sizeof(cfg_secret));
    prefs.end();
}

static void wifi_poll_task(void*) {
    for (;;) {
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
        // Fixed 60s cadence, matching the daemon's POLL_INTERVAL. This task never
        // blocks loop() (that's the whole point of running it here), so a failed
        // GET just retries next cycle with no BLE-reconnect-style urgency.
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

void wifi_poll_init(void) {
    load_config();
    // The poll task is always created (it's a no-op until cfg_url is set) so that
    // configuring WiFi for the first time via the wifi_ssid/.../wifi_url serial
    // commands takes effect immediately, with no reboot required.
    xTaskCreatePinnedToCore(wifi_poll_task, "wifi_poll", 8192, nullptr, 1, nullptr, 0);
    if (!cfg_ssid[0]) {
        Serial.println("WiFi poll: no SSID configured yet (BLE-only mode)");
        return;
    }
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg_ssid, cfg_pass);
    Serial.printf("WiFi poll: connecting to '%s'...\n", cfg_ssid);
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
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putString("ssid", cfg_ssid);
    prefs.end();
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.begin(cfg_ssid, cfg_pass);
    Serial.printf("WiFi ssid set to '%s'\n", cfg_ssid);
}

void wifi_poll_set_pass(const char* v) {
    strlcpy(cfg_pass, v, sizeof(cfg_pass));
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putString("pass", cfg_pass);
    prefs.end();
    if (cfg_ssid[0]) {
        WiFi.disconnect();
        WiFi.begin(cfg_ssid, cfg_pass);
    }
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

bool wifi_poll_is_connected(void) {
    return WiFi.status() == WL_CONNECTED;
}

const char* wifi_poll_get_ssid(void) {
    return cfg_ssid;
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
    Serial.printf("WiFi status: ssid='%s' connected=%d ip=%s url='%s' last_http=%d\n",
        cfg_ssid, WiFi.status() == WL_CONNECTED, WiFi.localIP().toString().c_str(),
        cfg_url, last_http_code);
}
