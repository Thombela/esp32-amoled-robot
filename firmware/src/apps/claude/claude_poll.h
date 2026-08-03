#pragma once

// Independent HTTP poller for the Claude app's usage %, decoupled from
// apps/settings/wifi/wifi_poll.h (the BLE/daemon usage pipeline) — a
// separate URL expecting the account usage API's own shape:
// {"five_hour": {"utilization_pct", "reset_minutes", "status"},
//  "seven_day": {"utilization_pct", "reset_minutes"}, ...}. Opt-in: does
// nothing until a URL is set via the Claude app's gear-icon settings screen
// (claude_settings.h).
void claude_poll_init(void);

void claude_poll_set_url(const char* url);
const char* claude_poll_get_url(void);  // "" if never configured

// Skips the rest of the current wait and fetches immediately — called by
// the Claude tile's refresh button.
void claude_poll_refresh_now(void);

// Call every loop(): if a fetch completed since the last call, parses the
// usage JSON and pushes it into the Claude tile via claude_screen_update();
// if a fetch failed, surfaces the reason via claude_screen_show_toast().
// A no-op otherwise.
void claude_poll_tick(void);
