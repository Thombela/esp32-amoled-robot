#include "claude_poll.h"
#include "claude.h"

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>

#include "../settings/wifi/wifi_poll.h"

#define NVS_NAMESPACE "clawd_claude"
#define POLL_INTERVAL_MS 60000
#define WAIT_STEP_MS 500

static char cfg_url[160] = {0};
static char rx_buf[384];
static volatile bool data_ready = false;
static volatile bool error_ready = false;
static volatile bool refresh_requested = false;
static char last_error[48] = {0};

static void load_config(void) {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);
    prefs.getString("url", cfg_url, sizeof(cfg_url));
    prefs.end();
}

static void do_fetch(void) {
    if (!cfg_url[0]) {
        strlcpy(last_error, "No URL configured", sizeof(last_error));
        error_ready = true;
        return;
    }
    if (!wifi_poll_is_connected()) {
        strlcpy(last_error, "WiFi not connected", sizeof(last_error));
        error_ready = true;
        return;
    }

    HTTPClient http;
    http.setTimeout(8000);
    // Plain http.begin(url) (no explicit client) auto-detects http:// vs
    // https:// from the URL itself — unlike wifi_poll.cpp's fetch, which
    // always assumes an https tunnel (ngrok), this endpoint is just as
    // likely a plain local dev server on the LAN (e.g. http://192.168.x.x:PORT).
    // Forcing WiFiClientSecure unconditionally (the earlier bug here) makes
    // a TLS handshake against a server that isn't speaking TLS at all,
    // which fails outright. For an https:// URL this still falls back to
    // skipping cert validation, matching wifi_poll.cpp's same trade-off.
    if (!http.begin(cfg_url)) {
        strlcpy(last_error, "Invalid URL", sizeof(last_error));
        error_ready = true;
        return;
    }

    int code = http.GET();
    if (code == 200) {
        String body = http.getString();
        size_t len = min(body.length(), sizeof(rx_buf) - 1);
        memcpy(rx_buf, body.c_str(), len);
        rx_buf[len] = '\0';
        data_ready = true;
    } else {
        snprintf(last_error, sizeof(last_error), "HTTP %d", code);
        error_ready = true;
    }
    Serial.printf("Claude poll: HTTP %d\n", code);
    http.end();
}

static void claude_poll_task(void*) {
    for (;;) {
        do_fetch();
        // Wait in short steps so a refresh request doesn't have to sit
        // through the rest of a 60s cycle.
        refresh_requested = false;
        for (int waited = 0; waited < POLL_INTERVAL_MS && !refresh_requested; waited += WAIT_STEP_MS) {
            vTaskDelay(pdMS_TO_TICKS(WAIT_STEP_MS));
        }
    }
}

void claude_poll_init(void) {
    load_config();
    xTaskCreatePinnedToCore(claude_poll_task, "claude_poll", 8192, nullptr, 1, nullptr, 0);
}

void claude_poll_set_url(const char* url) {
    strlcpy(cfg_url, url, sizeof(cfg_url));
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putString("url", cfg_url);
    prefs.end();
    Serial.printf("Claude poll url set to '%s'\n", cfg_url);
    refresh_requested = true;
}

const char* claude_poll_get_url(void) {
    return cfg_url;
}

void claude_poll_refresh_now(void) {
    refresh_requested = true;
}

void claude_poll_tick(void) {
    if (data_ready) {
        data_ready = false;

        // Real shape (from the account's own usage API):
        // {"five_hour": {"utilization_pct", "reset_minutes", "status"},
        //  "seven_day": {"utilization_pct", "reset_minutes"}, ...}. five_hour
        // is Anthropic's rolling session limit; seven_day is the weekly one.
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, rx_buf);
        if (err) {
            Serial.printf("Claude poll: JSON parse error: %s\n", err.c_str());
            claude_screen_show_toast("Invalid response", true);
        } else {
            float session_pct   = doc["five_hour"]["utilization_pct"] | 0.0f;
            int   session_reset = doc["five_hour"]["reset_minutes"]   | 0;
            float weekly_pct    = doc["seven_day"]["utilization_pct"] | 0.0f;
            int   weekly_reset  = doc["seven_day"]["reset_minutes"]   | 0;
            claude_screen_update(session_pct, session_reset, weekly_pct, weekly_reset);
        }
    }

    if (error_ready) {
        error_ready = false;
        claude_screen_show_toast(last_error, true);
    }
}
