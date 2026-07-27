#pragma once
#include "data.h"
#include "ble.h"

enum screen_t {
    SCREEN_HOME,
    SCREEN_CLAUDE,
    SCREEN_SETTINGS,
    SCREEN_COUNT,
};

void ui_init(void);
void ui_update(const UsageData* data);
void ui_tick(void);
void ui_show_screen(screen_t screen);
screen_t ui_get_current_screen(void);
void ui_update_ble_status(ble_state_t state, const char* name, const char* mac);
void ui_update_battery(int percent, bool charging);
void ui_refresh_settings(void);
