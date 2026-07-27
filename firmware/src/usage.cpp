#include "usage.h"

#include <Arduino.h>
#include <ArduinoJson.h>

#include "data.h"
#include "ui.h"
#include "usage_rate.h"
#include "hal/sound_hal.h"

static UsageData usage = {};

bool usage_apply_json(const char* json) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        Serial.printf("JSON parse error: %s\n", err.c_str());
        return false;
    }

    usage.session_pct = doc["s"] | 0.0f;
    usage.session_reset_mins = doc["sr"] | -1;
    usage.weekly_pct = doc["w"] | 0.0f;
    usage.weekly_reset_mins = doc["wr"] | -1;
    strlcpy(usage.status, doc["st"] | "unknown", sizeof(usage.status));
    usage.chime = doc["c"] | false;   // absent (old daemon / chime off) → stay silent
    const char* acct = doc["acct"] | "pro";
    usage.enterprise = (strcmp(acct, "ent") == 0);
    usage.time_pct = doc["tp"] | 0;
    usage.period_days = doc["pd"] | 30;
    strlcpy(usage.reset_date, doc["rd"] | "", sizeof(usage.reset_date));
    usage.clock_epoch = doc["t"] | 0L;
    usage.clock_fmt = doc["tf"] | 24;
    usage.ok = doc["ok"] | false;
    usage.valid = true;

    int g_before = usage_rate_group();
    bool session_reset = usage_rate_sample(usage.session_pct);
    int g_after = usage_rate_group();
    // 5-hour session limit refilled → chime so the user knows they can
    // use Claude again (no-op on boards without a buzzer). Gated on the
    // daemon's opt-in `chime` config; the `buzz` serial cmd ignores it.
    if (session_reset && usage.chime) {
        Serial.println("session reset detected — chime");
        sound_hal_play_reset();
    }
    if (g_after != g_before) {
        Serial.printf("usage rate: group %d -> %d (s=%.2f%%)\n",
            g_before, g_after, usage.session_pct);
    }
    ui_update(&usage);
    return true;
}
