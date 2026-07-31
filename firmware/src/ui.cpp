#include "ui.h"
#include <stdint.h>
#include <lvgl.h>
#include "icons.h"
#include "backgrounds.h"
#include "app_icons.h"
#include "brightness.h"
#include "wifi_poll.h"
#include "hal/board_caps.h"

// Custom fonts (scaled for 314 PPI, ~1.9x from original 165 PPI)
LV_FONT_DECLARE(font_tiempos_56);
LV_FONT_DECLARE(font_tiempos_34);
LV_FONT_DECLARE(font_styrene_28);
LV_FONT_DECLARE(font_styrene_24);
LV_FONT_DECLARE(font_styrene_20);
LV_FONT_DECLARE(font_styrene_14);
LV_FONT_DECLARE(font_styrene_12);

// Layout values computed from the active board's geometry. Populated once
// in ui_init() and treated as const for the rest of the program. Adding a
// new display size means extending compute_layout() with another
// breakpoint — never editing the screen-builder functions below.
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
};
static Layout L = {};

// Pick layout values from the active board's pixel dimensions. New ports
// inherit the closer breakpoint — visually OK, may need a polish pass for
// pixel-perfect alignment but never blocks the port from booting.
static void compute_layout(const BoardCaps& c) {
    L.scr_w = c.width;
    L.scr_h = c.height;
    L.margin = 20;
    L.title_y = 30;
    L.panel_pad_x = 16;
    L.panel_pad_y = 12;
    L.title_font = &font_tiempos_56;
    L.small_icons = false;
    L.batt_y = L.title_y;
    L.batt_w = ICON_BATTERY_W;

    if (c.height >= 460) {
        // Large layout — tuned for 480x480 (AMOLED-2.16).
        L.claude_label_font = &font_styrene_28;
        L.claude_pct_font   = &font_tiempos_56;
        L.claude_label1_y = 130; L.claude_pct1_y = 165;
        L.claude_label2_y = 290; L.claude_pct2_y = 325;
        L.settings_key_font   = &font_styrene_20;
        L.settings_value_font = &font_styrene_28;
        L.settings_row_h = 110; L.settings_row_gap = 16; L.settings_row1_y = 110;

        L.lib_card_y = 110; L.lib_card_h = 200; L.lib_card_gap = 16;
        L.lib_card_font = &font_styrene_20;

        L.popup_w = L.scr_w - 80; L.popup_h = 260; L.popup_list_y = 56;
        L.popup_row_h = 108; L.popup_row_gap = 12; L.popup_icon_size = 72;
        L.popup_title_font = &font_styrene_24;
        L.popup_row_font   = &font_styrene_20;
    } else if (c.height >= 300) {
        // Compact layout — tuned for 368x448 (AMOLED-1.8).
        L.claude_label_font = &font_styrene_20;
        L.claude_pct_font   = &font_tiempos_34;
        L.claude_label1_y = 100; L.claude_pct1_y = 130;
        L.claude_label2_y = 250; L.claude_pct2_y = 280;
        L.settings_key_font   = &font_styrene_14;
        L.settings_value_font = &font_styrene_20;
        L.settings_row_h = 90; L.settings_row_gap = 12; L.settings_row1_y = 90;

        L.lib_card_y = 90; L.lib_card_h = 170; L.lib_card_gap = 14;
        L.lib_card_font = &font_styrene_14;

        L.popup_w = L.scr_w - 40; L.popup_h = 220; L.popup_list_y = 44;
        L.popup_row_h = 86; L.popup_row_gap = 10; L.popup_icon_size = 56;
        L.popup_title_font = &font_styrene_20;
        L.popup_row_font   = &font_styrene_14;
    } else {
        // Small layout — tuned for 240x240 (LCD-1.54 and similar square TFTs).
        L.margin = 8;
        L.title_y = 4;
        L.panel_pad_x = 10;
        L.panel_pad_y = 6;
        L.title_font = &font_tiempos_34;
        L.small_icons = true;
        L.batt_y = 10;
        L.batt_w = ICON_BATTERY_SMALL_W;
        L.claude_label_font = &font_styrene_12;
        L.claude_pct_font   = &font_styrene_24;
        L.claude_label1_y = 40; L.claude_pct1_y = 60;
        L.claude_label2_y = 130; L.claude_pct2_y = 150;
        L.settings_key_font   = &font_styrene_12;
        L.settings_value_font = &font_styrene_14;
        L.settings_row_h = 50; L.settings_row_gap = 6; L.settings_row1_y = 40;

        L.lib_card_y = 34; L.lib_card_h = 100; L.lib_card_gap = 8;
        L.lib_card_font = &font_styrene_12;

        L.popup_w = L.scr_w - 20; L.popup_h = 140; L.popup_list_y = 30;
        L.popup_row_h = 56; L.popup_row_gap = 6; L.popup_icon_size = 34;
        L.popup_title_font = &font_styrene_14;
        L.popup_row_font   = &font_styrene_12;
    }

    L.content_w = L.scr_w - 2 * L.margin;
    L.lib_card_w = (L.content_w - L.lib_card_gap) / 2;
    // Icon max ~25% of the card block, not the whole card — leaves room for
    // the category label that now sits below (outside) the card itself.
    L.lib_icon_size = L.lib_card_h / 4;
    // Vertical distance from one row's card-top to the next row's card-top:
    // the card itself, the label gap+line below it, then the row gap.
    L.lib_row_stride = L.lib_card_h + 8 + L.lib_card_font->line_height + L.lib_card_gap;
}

// Anthropic brand palette — design tokens live in theme.h
#include "theme.h"
#define COL_BG        THEME_BG
#define COL_PANEL     THEME_PANEL
#define COL_TEXT      THEME_TEXT
#define COL_DIM       THEME_DIM
#define COL_ACCENT    THEME_ACCENT
#define COL_GREEN     THEME_GREEN
#define COL_AMBER     THEME_AMBER
#define COL_RED       THEME_RED
#define COL_BAR_BG    THEME_BAR_BG

// ---- Tileview: Home / Library ----
static lv_obj_t* tileview;
static lv_obj_t* tile_home;
static lv_obj_t* tile_library;
static screen_t  current_screen = SCREEN_HOME;

// ---- App screens (formerly tileview tiles, now full-screen overlays opened
// from the Library — see the app/category registry below) ----
static lv_obj_t* overlay_claude;
static lv_obj_t* overlay_settings;
static lv_obj_t* overlay_placeholder;
static lv_obj_t* lbl_placeholder_name;

// ---- Claude tile (static mock data for now) ----
static lv_obj_t* lbl_claude_daily_pct;
static lv_obj_t* lbl_claude_weekly_pct;

// ---- Settings tile ----
static lv_obj_t* lbl_settings_brightness_value;
static lv_obj_t* lbl_settings_ble_value;
static lv_obj_t* lbl_settings_wifi_value;

// ---- Battery indicator (shared, on top of every screen) ----
static lv_obj_t* battery_img;
static lv_image_dsc_t battery_dscs[5];  // empty, low, medium, full, charging

// ---- App / category registry ----
// The Library screen and its category popups are entirely data-driven off
// these tables so a new app is just an APPS[] row, not new screen-building
// code. Icon dscs are populated in ui_init(); entries within a category must
// stay alphabetically ordered — there's no runtime sort since a real sort is
// pointless at 1-2 apps/category. Social/Entertainment/Fitness/Photo are a
// testing scaffold (real logos, but SCREEN_PLACEHOLDER instead of a real
// screen) added to exercise the multi-category grid/scroll/gestures; remove
// once real apps land in those categories or they're no longer needed.
static lv_image_dsc_t icon_claude_dsc;
static lv_image_dsc_t icon_settings_dsc;
static lv_image_dsc_t icon_whatsapp_dsc;
static lv_image_dsc_t icon_apple_music_dsc;
static lv_image_dsc_t icon_strava_dsc;
static lv_image_dsc_t icon_photos_dsc;

enum AppCategory {
    CAT_UTILITIES,
    CAT_PRODUCTIVITY,
    CAT_SOCIAL,
    CAT_ENTERTAINMENT,
    CAT_FITNESS,
    CAT_PHOTO,
    CAT_CATEGORY_COUNT,
};

struct CategoryEntry { const char* name; AppCategory id; };
static const CategoryEntry CATEGORIES[] = {
    { "Utilities",     CAT_UTILITIES },
    { "Productivity",  CAT_PRODUCTIVITY },
    { "Social",        CAT_SOCIAL },
    { "Entertainment", CAT_ENTERTAINMENT },
    { "Fitness",       CAT_FITNESS },
    { "Photo",         CAT_PHOTO },
};
#define CATEGORY_COUNT ((int)(sizeof(CATEGORIES) / sizeof(CATEGORIES[0])))

struct AppEntry {
    const char*     name;
    AppCategory     category;
    screen_t        screen;
    lv_image_dsc_t* icon;
};
static AppEntry APPS[] = {
    { "Settings",     CAT_UTILITIES,     SCREEN_SETTINGS,    &icon_settings_dsc },
    { "Claude",       CAT_PRODUCTIVITY,  SCREEN_CLAUDE,      &icon_claude_dsc },
    { "WhatsApp",     CAT_SOCIAL,        SCREEN_PLACEHOLDER, &icon_whatsapp_dsc },
    { "Apple Music",  CAT_ENTERTAINMENT, SCREEN_PLACEHOLDER, &icon_apple_music_dsc },
    { "Strava",       CAT_FITNESS,       SCREEN_PLACEHOLDER, &icon_strava_dsc },
    { "Photos",       CAT_PHOTO,         SCREEN_PLACEHOLDER, &icon_photos_dsc },
};
#define APP_COUNT ((int)(sizeof(APPS) / sizeof(APPS[0])))

// ---- Category popup (folder contents) — built once, hidden until opened ----
static lv_obj_t* popup_scrim;
static lv_obj_t* popup_card;
static lv_obj_t* popup_title;
static lv_obj_t* popup_list;

// Forward declarations: card/row tap handlers and the Home<->Library nav
// helpers all reference each other across sections below.
static void open_category_popup(AppCategory cat);
static void open_app_screen(screen_t screen, const char* app_name);

static lv_color_t pct_color(float pct) {
    if (pct >= 80.0f) return COL_RED;
    if (pct >= 50.0f) return COL_AMBER;
    return COL_GREEN;
}

static lv_obj_t* make_panel(lv_obj_t* parent, int x, int y, int w, int h) {
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, w, h);
    lv_obj_set_style_bg_color(panel, COL_PANEL, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_left(panel, L.panel_pad_x, 0);
    lv_obj_set_style_pad_right(panel, L.panel_pad_x, 0);
    lv_obj_set_style_pad_top(panel, L.panel_pad_y, 0);
    lv_obj_set_style_pad_bottom(panel, L.panel_pad_y, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    // Panels (settings rows, the popup card) are clickable by default, which
    // would otherwise swallow a swipe gesture that happens to start on one —
    // let it bubble up to whichever ancestor has the actual gesture handler.
    lv_obj_add_flag(panel, LV_OBJ_FLAG_GESTURE_BUBBLE);
    return panel;
}

static void init_icon_dsc_rgb565a8(lv_image_dsc_t* dsc, int w, int h, const uint8_t* data) {
    dsc->header.w = w;
    dsc->header.h = h;
    dsc->header.cf = LV_COLOR_FORMAT_RGB565A8;
    dsc->header.stride = w * 2;
    dsc->data = data;
    dsc->data_size = w * h * 3;
}

// Opaque RGB565 (no alpha plane) — for a full-screen photo or a flat app
// icon, unlike the alpha-blended icon/logo helper above.
static void init_bg_dsc_rgb565(lv_image_dsc_t* dsc, int w, int h, const uint16_t* data) {
    dsc->header.w = w;
    dsc->header.h = h;
    dsc->header.cf = LV_COLOR_FORMAT_RGB565;
    dsc->header.stride = w * 2;
    dsc->data = (const uint8_t*)data;
    dsc->data_size = w * h * 2;
}

static void init_battery_icons(void) {
    if (L.small_icons) {
        init_icon_dsc_rgb565a8(&battery_dscs[0], ICON_BATTERY_SMALL_W, ICON_BATTERY_SMALL_H, icon_battery_small_data);
        init_icon_dsc_rgb565a8(&battery_dscs[1], ICON_BATTERY_LOW_SMALL_W, ICON_BATTERY_LOW_SMALL_H, icon_battery_low_small_data);
        init_icon_dsc_rgb565a8(&battery_dscs[2], ICON_BATTERY_MEDIUM_SMALL_W, ICON_BATTERY_MEDIUM_SMALL_H, icon_battery_medium_small_data);
        init_icon_dsc_rgb565a8(&battery_dscs[3], ICON_BATTERY_FULL_SMALL_W, ICON_BATTERY_FULL_SMALL_H, icon_battery_full_small_data);
        init_icon_dsc_rgb565a8(&battery_dscs[4], ICON_BATTERY_CHARGING_SMALL_W, ICON_BATTERY_CHARGING_SMALL_H, icon_battery_charging_small_data);
        return;
    }
    init_icon_dsc_rgb565a8(&battery_dscs[0], ICON_BATTERY_W, ICON_BATTERY_H, icon_battery_data);
    init_icon_dsc_rgb565a8(&battery_dscs[1], ICON_BATTERY_LOW_W, ICON_BATTERY_LOW_H, icon_battery_low_data);
    init_icon_dsc_rgb565a8(&battery_dscs[2], ICON_BATTERY_MEDIUM_W, ICON_BATTERY_MEDIUM_H, icon_battery_medium_data);
    init_icon_dsc_rgb565a8(&battery_dscs[3], ICON_BATTERY_FULL_W, ICON_BATTERY_FULL_H, icon_battery_full_data);
    init_icon_dsc_rgb565a8(&battery_dscs[4], ICON_BATTERY_CHARGING_W, ICON_BATTERY_CHARGING_H, icon_battery_charging_data);
}

// ======== Home tile: background photo, nothing else yet ========

static lv_image_dsc_t home_bg_dsc;

static void init_home_tile(lv_obj_t* tile) {
    lv_obj_set_style_bg_color(tile, COL_BG, 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tile, 0, 0);
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);

    init_bg_dsc_rgb565(&home_bg_dsc, BG_HOME_W, BG_HOME_H, bg_home_data);
    lv_obj_t* img = lv_image_create(tile);
    lv_image_set_src(img, &home_bg_dsc);
    lv_obj_set_pos(img, 0, 0);
}

// ======== Claude tile: minimal daily/weekly %, static mock data ========

static void init_claude_tile(lv_obj_t* tile) {
    lv_obj_set_style_bg_color(tile, COL_BG, 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tile, 0, 0);
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* l1 = lv_label_create(tile);
    lv_label_set_text(l1, "Daily");
    lv_obj_set_style_text_font(l1, L.claude_label_font, 0);
    lv_obj_set_style_text_color(l1, COL_DIM, 0);
    lv_obj_align(l1, LV_ALIGN_TOP_MID, 0, L.claude_label1_y);

    // Static mock — not wired to live UsageData yet (see ui_update() below).
    lbl_claude_daily_pct = lv_label_create(tile);
    lv_label_set_text(lbl_claude_daily_pct, "42%");
    lv_obj_set_style_text_font(lbl_claude_daily_pct, L.claude_pct_font, 0);
    lv_obj_set_style_text_color(lbl_claude_daily_pct, pct_color(42.0f), 0);
    lv_obj_align(lbl_claude_daily_pct, LV_ALIGN_TOP_MID, 0, L.claude_pct1_y);

    lv_obj_t* l2 = lv_label_create(tile);
    lv_label_set_text(l2, "Weekly");
    lv_obj_set_style_text_font(l2, L.claude_label_font, 0);
    lv_obj_set_style_text_color(l2, COL_DIM, 0);
    lv_obj_align(l2, LV_ALIGN_TOP_MID, 0, L.claude_label2_y);

    lbl_claude_weekly_pct = lv_label_create(tile);
    lv_label_set_text(lbl_claude_weekly_pct, "68%");
    lv_obj_set_style_text_font(lbl_claude_weekly_pct, L.claude_pct_font, 0);
    lv_obj_set_style_text_color(lbl_claude_weekly_pct, pct_color(68.0f), 0);
    lv_obj_align(lbl_claude_weekly_pct, LV_ALIGN_TOP_MID, 0, L.claude_pct2_y);
}

// ======== Settings tile: real brightness / BLE / WiFi status ========

static void init_settings_tile(lv_obj_t* tile) {
    lv_obj_set_style_bg_color(tile, COL_BG, 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tile, 0, 0);
    lv_obj_set_style_pad_all(tile, 0, 0);
    // Vertical-only: a real scrollbar + bounce-at-the-end feedback makes it
    // obvious there are exactly 3 rows and nothing is stuck, instead of a
    // dead, unresponsive screen. Horizontal stays free for the swipe-left
    // "back to Library" gesture (see back_gesture_cb).
    lv_obj_add_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(tile, LV_DIR_VER);

    lv_obj_t* title = lv_label_create(tile);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_font(title, L.title_font, 0);
    lv_obj_set_style_text_color(title, COL_TEXT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, L.title_y);

    int y = L.settings_row1_y;

    lv_obj_t* p1 = make_panel(tile, L.margin, y, L.content_w, L.settings_row_h);
    lv_obj_t* k1 = lv_label_create(p1);
    lv_label_set_text(k1, "BRIGHTNESS");
    lv_obj_set_style_text_font(k1, L.settings_key_font, 0);
    lv_obj_set_style_text_color(k1, COL_DIM, 0);
    lv_obj_set_pos(k1, 0, 0);
    lbl_settings_brightness_value = lv_label_create(p1);
    lv_obj_set_style_text_font(lbl_settings_brightness_value, L.settings_value_font, 0);
    lv_obj_set_style_text_color(lbl_settings_brightness_value, COL_TEXT, 0);
    lv_obj_set_pos(lbl_settings_brightness_value, 0, 30);
    y += L.settings_row_h + L.settings_row_gap;

    lv_obj_t* p2 = make_panel(tile, L.margin, y, L.content_w, L.settings_row_h);
    lv_obj_t* k2 = lv_label_create(p2);
    lv_label_set_text(k2, "BLUETOOTH");
    lv_obj_set_style_text_font(k2, L.settings_key_font, 0);
    lv_obj_set_style_text_color(k2, COL_DIM, 0);
    lv_obj_set_pos(k2, 0, 0);
    lbl_settings_ble_value = lv_label_create(p2);
    lv_obj_set_style_text_font(lbl_settings_ble_value, L.settings_value_font, 0);
    lv_obj_set_style_text_color(lbl_settings_ble_value, COL_TEXT, 0);
    lv_obj_set_pos(lbl_settings_ble_value, 0, 30);
    y += L.settings_row_h + L.settings_row_gap;

    lv_obj_t* p3 = make_panel(tile, L.margin, y, L.content_w, L.settings_row_h);
    lv_obj_t* k3 = lv_label_create(p3);
    lv_label_set_text(k3, "WI-FI");
    lv_obj_set_style_text_font(k3, L.settings_key_font, 0);
    lv_obj_set_style_text_color(k3, COL_DIM, 0);
    lv_obj_set_pos(k3, 0, 0);
    lbl_settings_wifi_value = lv_label_create(p3);
    lv_obj_set_style_text_font(lbl_settings_wifi_value, L.settings_value_font, 0);
    lv_obj_set_style_text_color(lbl_settings_wifi_value, COL_TEXT, 0);
    lv_obj_set_pos(lbl_settings_wifi_value, 0, 30);

    ui_refresh_settings();
}

void ui_refresh_settings(void) {
    if (!lbl_settings_brightness_value) return;  // tile not built yet

    int pct = brightness_get() * 100 / 255;
    lv_label_set_text_fmt(lbl_settings_brightness_value, "%d%%", pct);

    ble_state_t bs = ble_get_state();
    lv_obj_set_style_text_color(lbl_settings_ble_value,
        bs == BLE_STATE_CONNECTED ? COL_GREEN : COL_DIM, 0);
    if (bs == BLE_STATE_CONNECTED) {
        const char* name = ble_get_device_name();
        lv_label_set_text_fmt(lbl_settings_ble_value, "Connected\n%s",
            name[0] ? name : ble_get_mac_address());
    } else if (bs == BLE_STATE_ADVERTISING) {
        lv_label_set_text(lbl_settings_ble_value, "Advertising");
    } else if (bs == BLE_STATE_DISCONNECTED) {
        lv_label_set_text(lbl_settings_ble_value, "Disconnected");
    } else {
        lv_label_set_text(lbl_settings_ble_value, "Initializing");
    }

    bool wifi_ok = wifi_poll_is_connected();
    lv_obj_set_style_text_color(lbl_settings_wifi_value, wifi_ok ? COL_GREEN : COL_DIM, 0);
    const char* ssid = wifi_poll_get_ssid();
    if (!ssid[0]) {
        lv_label_set_text(lbl_settings_wifi_value, "Not configured");
    } else if (wifi_ok) {
        lv_label_set_text_fmt(lbl_settings_wifi_value, "%s\n%s", ssid, wifi_poll_get_ip());
    } else {
        lv_label_set_text_fmt(lbl_settings_wifi_value, "%s\nConnecting...", ssid);
    }
}

// ======== Placeholder tile: stub screen for apps with no real UI yet ========
// (Social/Entertainment/Fitness/Photo — a testing scaffold, see the app/
// category registry comment above.) One shared overlay for all of them;
// open_app_screen() sets lbl_placeholder_name to whichever app was tapped.

static void init_placeholder_tile(lv_obj_t* tile) {
    lv_obj_set_style_bg_color(tile, COL_BG, 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tile, 0, 0);
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* hint = lv_label_create(tile);
    lv_label_set_text(hint, "Coming soon");
    lv_obj_set_style_text_font(hint, L.claude_label_font, 0);
    lv_obj_set_style_text_color(hint, COL_DIM, 0);
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, -20);

    lbl_placeholder_name = lv_label_create(tile);
    lv_obj_set_style_text_font(lbl_placeholder_name, L.title_font, 0);
    lv_obj_set_style_text_color(lbl_placeholder_name, COL_TEXT, 0);
    lv_obj_align(lbl_placeholder_name, LV_ALIGN_CENTER, 0, 20);
}

// ======== Library tile: blurred bg + category folder-card grid ========

static lv_image_dsc_t library_bg_dsc;

// App icons are baked with their alpha channel (see app_icons.h) — each PNG
// already has its own rounded-square shape and transparent margins, so
// there's no need to wrap/clip it into a squircle mask like the old opaque
// JPEG icons needed. Scaled from the single baked-resolution source (every
// icon is baked at the same size) rather than baking multiple sizes.
// Returns the image unpositioned — callers place it (card icons are
// top-centered, popup row icons are top-centered too) via lv_obj_align.
static lv_obj_t* make_app_icon(lv_obj_t* parent, const lv_image_dsc_t* src, int size, int src_w) {
    lv_obj_t* img = lv_image_create(parent);
    lv_image_set_src(img, src);
    lv_image_set_scale(img, 256 * size / src_w);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
    return img;
}

static void category_card_clicked_cb(lv_event_t* e) {
    AppCategory cat = (AppCategory)(intptr_t)lv_event_get_user_data(e);
    open_category_popup(cat);
}

static void init_library_tile(lv_obj_t* tile) {
    lv_obj_set_style_bg_color(tile, COL_BG, 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tile, 0, 0);
    lv_obj_set_style_pad_all(tile, 0, 0);
    // Vertical-only, same reasoning as Settings: a real scrollbar/bounce
    // makes a 6-category (2x3) grid obviously scrollable instead of dead,
    // and horizontal stays free for the tileview's own Home<->Library page.
    lv_obj_add_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(tile, LV_DIR_VER);

    init_bg_dsc_rgb565(&library_bg_dsc, BG_LIBRARY_W, BG_LIBRARY_H, bg_library_data);
    lv_obj_t* bg = lv_image_create(tile);
    lv_image_set_src(bg, &library_bg_dsc);
    lv_obj_set_pos(bg, 0, 0);
    lv_obj_clear_flag(bg, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* title = lv_label_create(tile);
    lv_label_set_text(title, "Library");
    lv_obj_set_style_text_font(title, L.title_font, 0);
    lv_obj_set_style_text_color(title, COL_TEXT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, L.title_y);

    for (int i = 0; i < CATEGORY_COUNT; i++) {
        int col = i % 2;
        int row = i / 2;
        int x = L.margin + col * (L.lib_card_w + L.lib_card_gap);
        int y = L.lib_card_y + row * L.lib_row_stride;
        lv_obj_t* card = make_panel(tile, x, y, L.lib_card_w, L.lib_card_h);
        // Frosted-glass look: translucent panel over the blurred bg behind it.
        lv_obj_set_style_bg_opa(card, LV_OPA_70, 0);
        lv_obj_set_style_radius(card, 20, 0);
        lv_obj_add_event_cb(card, category_card_clicked_cb, LV_EVENT_CLICKED,
                             (void*)(intptr_t)CATEGORIES[i].id);

        // Folder preview: the first (alphabetically) app in this category.
        for (int j = 0; j < APP_COUNT; j++) {
            if (APPS[j].category != CATEGORIES[i].id) continue;
            lv_obj_t* icon = make_app_icon(card, APPS[j].icon, L.lib_icon_size, APP_ICON_CLAUDE_W);
            lv_obj_center(icon);
            break;
        }

        // Category name sits below (outside) the card block, like an app
        // name under its icon, rather than inside it.
        lv_obj_t* label = lv_label_create(tile);
        lv_label_set_text(label, CATEGORIES[i].name);
        lv_obj_set_style_text_font(label, L.lib_card_font, 0);
        lv_obj_set_style_text_color(label, COL_TEXT, 0);
        lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(label, L.lib_card_w);
        lv_obj_set_pos(label, x, y + L.lib_card_h + 8);
    }
}

// ======== Category popup: tap a folder card -> shows its apps, tap a row ==
// ======== to open it, tap the dimmed scrim to dismiss ====================

static void app_row_clicked_cb(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    lv_obj_add_flag(popup_scrim, LV_OBJ_FLAG_HIDDEN);
    open_app_screen(APPS[idx].screen, APPS[idx].name);
}

static void popup_scrim_clicked_cb(lv_event_t* e) {
    (void)e;
    lv_obj_add_flag(popup_scrim, LV_OBJ_FLAG_HIDDEN);
}

static void open_category_popup(AppCategory cat) {
    const char* cat_name = "";
    for (int i = 0; i < CATEGORY_COUNT; i++) {
        if (CATEGORIES[i].id == cat) cat_name = CATEGORIES[i].name;
    }
    lv_label_set_text(popup_title, cat_name);

    lv_obj_clean(popup_list);
    int y = 0;
    // APPS[] entries are kept alphabetically ordered per category at the
    // source, so iterating in array order already yields alphabetical rows.
    for (int i = 0; i < APP_COUNT; i++) {
        if (APPS[i].category != cat) continue;

        lv_obj_t* row = lv_obj_create(popup_list);
        lv_obj_set_pos(row, 0, y);
        lv_obj_set_size(row, L.popup_w - 2 * L.panel_pad_x, L.popup_row_h);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(row, app_row_clicked_cb, LV_EVENT_CLICKED,
                             (void*)(intptr_t)i);

        // Icon on top, name centered underneath — same tile shape as the
        // Library category cards, just smaller.
        lv_obj_t* icon = make_app_icon(row, APPS[i].icon, L.popup_icon_size, APP_ICON_CLAUDE_W);
        lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 0);

        lv_obj_t* name = lv_label_create(row);
        lv_label_set_text(name, APPS[i].name);
        lv_obj_set_style_text_font(name, L.popup_row_font, 0);
        lv_obj_set_style_text_color(name, COL_TEXT, 0);
        lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(name, L.popup_w - 2 * L.panel_pad_x);
        lv_obj_set_pos(name, 0, L.popup_icon_size + 8);

        y += L.popup_row_h + L.popup_row_gap;
    }

    lv_obj_clear_flag(popup_scrim, LV_OBJ_FLAG_HIDDEN);
}

static void init_category_popup(lv_obj_t* scr) {
    popup_scrim = lv_obj_create(scr);
    lv_obj_set_pos(popup_scrim, 0, 0);
    lv_obj_set_size(popup_scrim, L.scr_w, L.scr_h);
    lv_obj_set_style_bg_color(popup_scrim, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(popup_scrim, LV_OPA_60, 0);
    lv_obj_set_style_border_width(popup_scrim, 0, 0);
    lv_obj_set_style_radius(popup_scrim, 0, 0);
    lv_obj_set_style_pad_all(popup_scrim, 0, 0);
    lv_obj_clear_flag(popup_scrim, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(popup_scrim, popup_scrim_clicked_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(popup_scrim, LV_OBJ_FLAG_HIDDEN);

    popup_card = make_panel(popup_scrim, (L.scr_w - L.popup_w) / 2, (L.scr_h - L.popup_h) / 2,
                             L.popup_w, L.popup_h);
    lv_obj_set_style_radius(popup_card, 20, 0);

    popup_title = lv_label_create(popup_card);
    lv_obj_set_style_text_font(popup_title, L.popup_title_font, 0);
    lv_obj_set_style_text_color(popup_title, COL_TEXT, 0);
    lv_obj_set_style_text_align(popup_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(popup_title, L.popup_w - 2 * L.panel_pad_x);
    lv_obj_set_pos(popup_title, 0, 0);

    popup_list = lv_obj_create(popup_card);
    lv_obj_set_style_bg_opa(popup_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(popup_list, 0, 0);
    lv_obj_set_style_pad_all(popup_list, 0, 0);
    lv_obj_clear_flag(popup_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(popup_list, 0, L.popup_list_y);
    lv_obj_set_size(popup_list, L.popup_w - 2 * L.panel_pad_x,
                     L.popup_h - 2 * L.panel_pad_y - L.popup_list_y);
}

// ======== Navigation between Home / Library / popup / app screens ========

static void close_to_library(void) {
    lv_obj_add_flag(popup_scrim, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(overlay_claude, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(overlay_settings, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(overlay_placeholder, LV_OBJ_FLAG_HIDDEN);
    lv_tileview_set_tile_by_index(tileview, 1, 0, LV_ANIM_OFF);
    current_screen = SCREEN_LIBRARY;
}

// Both app screens are full-screen with no chrome of their own, so "back" is
// gesture-only: swipe left anywhere (bubbled up from any clickable child via
// LV_OBJ_FLAG_GESTURE_BUBBLE, e.g. the settings row panels), or tap/swipe-up
// the home-indicator bar — mirroring iOS's home-indicator swipe-to-exit.
static void back_gesture_cb(lv_event_t* e) {
    (void)e;
    lv_indev_t* indev = lv_indev_active();
    if (!indev) return;
    if (lv_indev_get_gesture_dir(indev) & LV_DIR_LEFT) close_to_library();
}

static void home_indicator_cb(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        close_to_library();
        return;
    }
    lv_indev_t* indev = lv_indev_active();
    if (!indev) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    if (dir & (LV_DIR_TOP | LV_DIR_LEFT)) close_to_library();
}

// A centered, 50%-wide bar at the bottom of the screen — tap it, or swipe up
// from it, to return to the Library grid. zone_h (the touch target, and the
// space init_*_tile's content area leaves clear above it) is capped at 10%
// of screen height; the visible bar itself stays a slim iOS-style pill
// within that target rather than filling the whole target with color.
static void make_home_indicator(lv_obj_t* parent, int zone_h) {
    int zone_w = L.scr_w / 2;
    lv_obj_t* zone = lv_obj_create(parent);
    lv_obj_set_style_bg_opa(zone, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(zone, 0, 0);
    lv_obj_set_style_pad_all(zone, 0, 0);
    lv_obj_clear_flag(zone, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(zone, zone_w, zone_h);
    lv_obj_set_pos(zone, (L.scr_w - zone_w) / 2, L.scr_h - zone_h);
    lv_obj_add_event_cb(zone, home_indicator_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(zone, home_indicator_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* bar = lv_obj_create(zone);
    lv_obj_set_style_bg_color(bar, COL_TEXT, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_40, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(bar, zone_w, 4);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, -6);
}

static void open_app_screen(screen_t screen, const char* app_name) {
    lv_obj_add_flag(popup_scrim, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(overlay_claude, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(overlay_settings, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(overlay_placeholder, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* target = overlay_placeholder;
    if (screen == SCREEN_CLAUDE) target = overlay_claude;
    else if (screen == SCREEN_SETTINGS) target = overlay_settings;

    if (screen == SCREEN_PLACEHOLDER) lv_label_set_text(lbl_placeholder_name, app_name);
    lv_obj_clear_flag(target, LV_OBJ_FLAG_HIDDEN);
    current_screen = screen;
    if (screen == SCREEN_SETTINGS) ui_refresh_settings();
}

// ======== Public API ========

static void tileview_changed_cb(lv_event_t* e) {
    (void)e;
    lv_obj_t* act = lv_tileview_get_tile_active(tileview);
    current_screen = (act == tile_library) ? SCREEN_LIBRARY : SCREEN_HOME;
}

void ui_init(void) {
    compute_layout(board_caps());

    lv_obj_t* scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    init_battery_icons();
    init_icon_dsc_rgb565a8(&icon_claude_dsc, APP_ICON_CLAUDE_W, APP_ICON_CLAUDE_H, app_icon_claude_data);
    init_icon_dsc_rgb565a8(&icon_settings_dsc, APP_ICON_SETTINGS_W, APP_ICON_SETTINGS_H, app_icon_settings_data);
    init_icon_dsc_rgb565a8(&icon_whatsapp_dsc, APP_ICON_WHATSAPP_W, APP_ICON_WHATSAPP_H, app_icon_whatsapp_data);
    init_icon_dsc_rgb565a8(&icon_apple_music_dsc, APP_ICON_APPLE_MUSIC_W, APP_ICON_APPLE_MUSIC_H, app_icon_apple_music_data);
    init_icon_dsc_rgb565a8(&icon_strava_dsc, APP_ICON_STRAVA_W, APP_ICON_STRAVA_H, app_icon_strava_data);
    init_icon_dsc_rgb565a8(&icon_photos_dsc, APP_ICON_PHOTOS_W, APP_ICON_PHOTOS_H, app_icon_photos_data);

    tileview = lv_tileview_create(scr);
    lv_obj_set_style_bg_opa(tileview, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tileview, 0, 0);
    lv_obj_set_style_pad_all(tileview, 0, 0);

    tile_home    = lv_tileview_add_tile(tileview, 0, 0, LV_DIR_HOR);
    tile_library = lv_tileview_add_tile(tileview, 1, 0, LV_DIR_HOR);

    init_home_tile(tile_home);
    init_library_tile(tile_library);

    lv_obj_add_event_cb(tileview, tileview_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // App screens: full-screen overlays on top of the tileview, hidden until
    // opened from a Library category popup (or ui_show_screen()). Content
    // lives in a sub-object sized to leave the home-indicator bar's touch
    // target (see make_home_indicator) free — keeps that strip out of
    // Settings' own scrollable area so the two gestures never fight over
    // the touch.
    int zone_h = L.scr_h / 10;  // touch target, capped at 10% of screen height

    overlay_claude = lv_obj_create(scr);
    lv_obj_set_pos(overlay_claude, 0, 0);
    lv_obj_set_size(overlay_claude, L.scr_w, L.scr_h);
    lv_obj_set_style_bg_color(overlay_claude, COL_BG, 0);
    lv_obj_set_style_bg_opa(overlay_claude, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(overlay_claude, 0, 0);
    lv_obj_set_style_pad_all(overlay_claude, 0, 0);
    lv_obj_clear_flag(overlay_claude, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* claude_content = lv_obj_create(overlay_claude);
    lv_obj_set_pos(claude_content, 0, 0);
    lv_obj_set_size(claude_content, L.scr_w, L.scr_h - zone_h);
    init_claude_tile(claude_content);
    lv_obj_add_event_cb(overlay_claude, back_gesture_cb, LV_EVENT_GESTURE, NULL);
    make_home_indicator(overlay_claude, zone_h);
    lv_obj_add_flag(overlay_claude, LV_OBJ_FLAG_HIDDEN);

    overlay_settings = lv_obj_create(scr);
    lv_obj_set_pos(overlay_settings, 0, 0);
    lv_obj_set_size(overlay_settings, L.scr_w, L.scr_h);
    lv_obj_set_style_bg_color(overlay_settings, COL_BG, 0);
    lv_obj_set_style_bg_opa(overlay_settings, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(overlay_settings, 0, 0);
    lv_obj_set_style_pad_all(overlay_settings, 0, 0);
    lv_obj_clear_flag(overlay_settings, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* settings_content = lv_obj_create(overlay_settings);
    lv_obj_set_pos(settings_content, 0, 0);
    lv_obj_set_size(settings_content, L.scr_w, L.scr_h - zone_h);
    init_settings_tile(settings_content);
    lv_obj_add_event_cb(overlay_settings, back_gesture_cb, LV_EVENT_GESTURE, NULL);
    make_home_indicator(overlay_settings, zone_h);
    lv_obj_add_flag(overlay_settings, LV_OBJ_FLAG_HIDDEN);

    overlay_placeholder = lv_obj_create(scr);
    lv_obj_set_pos(overlay_placeholder, 0, 0);
    lv_obj_set_size(overlay_placeholder, L.scr_w, L.scr_h);
    lv_obj_set_style_bg_color(overlay_placeholder, COL_BG, 0);
    lv_obj_set_style_bg_opa(overlay_placeholder, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(overlay_placeholder, 0, 0);
    lv_obj_set_style_pad_all(overlay_placeholder, 0, 0);
    lv_obj_clear_flag(overlay_placeholder, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* placeholder_content = lv_obj_create(overlay_placeholder);
    lv_obj_set_pos(placeholder_content, 0, 0);
    lv_obj_set_size(placeholder_content, L.scr_w, L.scr_h - zone_h);
    init_placeholder_tile(placeholder_content);
    lv_obj_add_event_cb(overlay_placeholder, back_gesture_cb, LV_EVENT_GESTURE, NULL);
    make_home_indicator(overlay_placeholder, zone_h);
    lv_obj_add_flag(overlay_placeholder, LV_OBJ_FLAG_HIDDEN);

    // Category popup — also a full-screen overlay (scrim + centered card).
    init_category_popup(scr);

    // Battery indicator is created last so it paints on top of every
    // screen/overlay, not just the tileview tiles.
    battery_img = lv_image_create(scr);
    lv_image_set_src(battery_img, &battery_dscs[0]);
    lv_obj_set_pos(battery_img, L.scr_w - L.batt_w - L.margin, L.batt_y);
    // Boards without battery telemetry never show the indicator (per the HAL
    // contract; previously every board drew the empty-battery glyph).
    if (!board_caps().has_battery) {
        lv_obj_del(battery_img);
        battery_img = nullptr;
    }
}

void ui_update(const UsageData* data) {
    (void)data;
    // Claude tile shows static mock daily/weekly % (see init_claude_tile) — not
    // wired to live data yet, per product decision (deferred until the user's
    // own web-API/WiFi setup is finished). Wire data->session_pct/weekly_pct
    // into lbl_claude_daily_pct/lbl_claude_weekly_pct here when ready.
}

void ui_tick(void) {
    // WiFi has no push-on-change event today (unlike brightness/BLE, which
    // already trigger their own refresh on state change) — poll it at ~1Hz
    // while the user is actually looking at Settings.
    static uint32_t last_ms = 0;
    if (current_screen != SCREEN_SETTINGS) return;
    uint32_t now = lv_tick_get();
    if (now - last_ms < 1000) return;
    last_ms = now;
    ui_refresh_settings();
}

void ui_show_screen(screen_t screen) {
    lv_obj_add_flag(popup_scrim, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(overlay_claude, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(overlay_settings, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(overlay_placeholder, LV_OBJ_FLAG_HIDDEN);

    if (screen == SCREEN_CLAUDE || screen == SCREEN_SETTINGS) {
        lv_tileview_set_tile_by_index(tileview, 1, 0, LV_ANIM_OFF);
        open_app_screen(screen, "");
        return;
    }
    uint32_t col = (screen == SCREEN_LIBRARY) ? 1 : 0;
    lv_tileview_set_tile_by_index(tileview, col, 0, LV_ANIM_OFF);
    current_screen = screen;
}

screen_t ui_get_current_screen(void) {
    return current_screen;
}

void ui_update_ble_status(ble_state_t state, const char* name, const char* mac) {
    (void)state; (void)name; (void)mac;
    ui_refresh_settings();  // single formatting source of truth re-reads live via ble_get_*()
}

void ui_update_battery(int percent, bool charging) {
    if (!battery_img) return;
    int idx;
    if (charging) {
        idx = 4;
    } else if (percent < 0) {
        idx = 0;
    } else if (percent <= 10) {
        idx = 0;
    } else if (percent <= 35) {
        idx = 1;
    } else if (percent <= 75) {
        idx = 2;
    } else {
        idx = 3;
    }
    lv_image_set_src(battery_img, &battery_dscs[idx]);
}
