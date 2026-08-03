#pragma once
#include <lvgl.h>

// Claude app screen: daily/weekly % display, a gear icon opening the
// data-source settings screen (claude_settings.h), and a refresh button.
void claude_screen_init(lv_obj_t* tile);

// Pushes a fresh reading into the tile's labels — called by claude_poll.cpp
// whenever a new fetch completes. `*_reset_min` is minutes until that
// window's limit resets.
void claude_screen_update(float session_pct, int session_reset_min,
                           float weekly_pct, int weekly_reset_min);

// Shows a temporary status message (e.g. a fetch failure) near the bottom
// of the tile; auto-hides after a few seconds. `is_error` picks red vs. a
// neutral dim style. Called by claude_poll.cpp and the refresh button.
void claude_screen_show_toast(const char* msg, bool is_error);

// Call every loop() (via ui_tick()): hides the toast once its time is up.
void claude_screen_tick(void);
