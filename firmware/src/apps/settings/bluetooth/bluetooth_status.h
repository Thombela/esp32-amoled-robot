#pragma once
#include <lvgl.h>

// Formats the live BLE connection state into the Settings screen's
// Bluetooth row value label. Called on every settings_screen_refresh().
void bluetooth_status_refresh(lv_obj_t* lbl_value);
