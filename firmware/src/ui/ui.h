#pragma once
#include <lvgl.h>
#include "../data.h"
#include "../core/ble/ble.h"
#include "theme/theme.h"

enum screen_t {
    SCREEN_HOME,
    SCREEN_LIBRARY,
    SCREEN_CLAUDE,
    SCREEN_SETTINGS,
    SCREEN_PLACEHOLDER,   // stub screen for apps with no real UI yet (testing only)
    SCREEN_WIFI_SETUP,    // covers both the network list and credential entry
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

// WiFi setup navigation (apps/settings/wifi/wifi_setup.cpp owns the screen
// content; ui.cpp owns overlay visibility/back-navigation, same split as
// every other app screen). `secured`/`enterprise` describe the network the
// user tapped in the list, so the credential screen knows which fields to
// show (open network: neither; PSK: password only; enterprise: both).
void ui_open_wifi_setup(void);
void ui_open_wifi_credentials(const char* ssid, bool secured, bool enterprise);
void ui_close_wifi_credentials(void);

// Claude app's data-source settings (gear icon on the Claude tile).
void ui_open_claude_settings(void);

// ---- Shared toolkit for apps/<name>/ screen-builder code ----

// Anthropic brand palette — design tokens live in theme.h.
#define COL_BG        THEME_BG
#define COL_PANEL     THEME_PANEL
#define COL_TEXT      THEME_TEXT
#define COL_DIM       THEME_DIM
#define COL_ACCENT    THEME_ACCENT
#define COL_GREEN     THEME_GREEN
#define COL_AMBER     THEME_AMBER
#define COL_RED       THEME_RED
#define COL_BAR_BG    THEME_BAR_BG

// Layout values computed once in ui_init() from the active board's geometry
// (see compute_layout() in ui.cpp) and treated as read-only everywhere else.
// Defined (non-static) in ui.cpp so apps/claude and apps/settings can read it.
struct Layout {
    int16_t scr_w, scr_h;
    int16_t margin;
    int16_t title_y;
    int16_t content_w;
    int16_t panel_pad_x, panel_pad_y;
    const lv_font_t* title_font;
    bool    small_icons;             // 24px battery (vs 48px) on small screens
    int16_t batt_y;                  // battery icon top edge
    int16_t batt_w;                  // battery icon width, for position math

    // Claude tile (minimal daily/weekly %)
    const lv_font_t* claude_label_font;
    const lv_font_t* claude_pct_font;
    int16_t claude_label1_y, claude_pct1_y;
    int16_t claude_label2_y, claude_pct2_y;

    // Settings tile
    const lv_font_t* settings_key_font;
    const lv_font_t* settings_value_font;
    int16_t settings_row_h, settings_row_gap, settings_row1_y;

    // Library tile: 2-column category card grid (derived lib_card_w/lib_row_stride below)
    int16_t lib_card_y, lib_card_h, lib_card_gap, lib_card_w;
    int16_t lib_icon_size, lib_row_stride;
    const lv_font_t* lib_card_font;

    // Category popup (folder contents)
    int16_t popup_w, popup_h, popup_list_y;
    int16_t popup_row_h, popup_row_gap, popup_icon_size;
    const lv_font_t* popup_title_font;
    const lv_font_t* popup_row_font;

    // WiFi setup: network list + credential entry (apps/settings/wifi/wifi_setup.cpp)
    const lv_font_t* wifi_row_font;
    int16_t wifi_row_h, wifi_list_y;
    const lv_font_t* wifi_field_font;
    int16_t wifi_field_h, wifi_field_gap, wifi_fields_y;
    int16_t wifi_kb_h;
};
extern Layout L;

// A small rounded, flat-color panel — the one repeated "card" look used by
// both the Settings rows and the Library category cards/popup.
lv_obj_t* ui_make_panel(lv_obj_t* parent, int x, int y, int w, int h);
