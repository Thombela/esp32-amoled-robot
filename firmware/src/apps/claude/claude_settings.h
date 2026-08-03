#pragma once
#include <lvgl.h>

// Claude app settings: set the URL the app fetches {daily, weekly} usage %
// from (apps/claude/claude_poll.h owns the actual fetch/parse). Opened via
// the gear icon on the Claude tile.
void claude_settings_screen_init(lv_obj_t* tile);

// Refreshes the URL field from saved config; called by ui_open_claude_settings()
// every time the screen is opened.
void claude_settings_screen_open(void);
