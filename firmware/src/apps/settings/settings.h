#pragma once
#include <lvgl.h>

// Settings app screen: brightness / Bluetooth / WiFi status rows.
void settings_screen_init(lv_obj_t* tile);

// Re-reads brightness/BLE/WiFi state and updates the row labels in place.
// Safe to call before settings_screen_init() (no-ops until the tile is built).
void settings_screen_refresh(void);
