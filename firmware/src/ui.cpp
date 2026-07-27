#include "ui.h"
#include <lvgl.h>
#include "icons.h"
#include "backgrounds.h"
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
    } else if (c.height >= 300) {
        // Compact layout — tuned for 368x448 (AMOLED-1.8).
        L.claude_label_font = &font_styrene_20;
        L.claude_pct_font   = &font_tiempos_34;
        L.claude_label1_y = 100; L.claude_pct1_y = 130;
        L.claude_label2_y = 250; L.claude_pct2_y = 280;
        L.settings_key_font   = &font_styrene_14;
        L.settings_value_font = &font_styrene_20;
        L.settings_row_h = 90; L.settings_row_gap = 12; L.settings_row1_y = 90;
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
    }

    L.content_w = L.scr_w - 2 * L.margin;
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

// ---- Tileview: Home / Claude / Settings ----
static lv_obj_t* tileview;
static lv_obj_t* tile_home;
static lv_obj_t* tile_claude;
static lv_obj_t* tile_settings;
static screen_t  current_screen = SCREEN_HOME;

// ---- Claude tile (static mock data for now) ----
static lv_obj_t* lbl_claude_daily_pct;
static lv_obj_t* lbl_claude_weekly_pct;

// ---- Settings tile ----
static lv_obj_t* lbl_settings_brightness_value;
static lv_obj_t* lbl_settings_ble_value;
static lv_obj_t* lbl_settings_wifi_value;

// ---- Battery indicator (shared, on top of every tile) ----
static lv_obj_t* battery_img;
static lv_image_dsc_t battery_dscs[5];  // empty, low, medium, full, charging

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

// Opaque RGB565 (no alpha plane) — for a full-screen photo, unlike the
// alpha-blended icon/logo helper above.
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
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);

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

// ======== Public API ========

static void tileview_changed_cb(lv_event_t* e) {
    (void)e;
    lv_obj_t* act = lv_tileview_get_tile_active(tileview);
    current_screen = (act == tile_claude)   ? SCREEN_CLAUDE
                    : (act == tile_settings) ? SCREEN_SETTINGS
                                              : SCREEN_HOME;
    if (current_screen == SCREEN_SETTINGS) ui_refresh_settings();
}

void ui_init(void) {
    compute_layout(board_caps());

    lv_obj_t* scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, COL_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    init_battery_icons();

    tileview = lv_tileview_create(scr);
    lv_obj_set_style_bg_opa(tileview, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tileview, 0, 0);
    lv_obj_set_style_pad_all(tileview, 0, 0);

    tile_home     = lv_tileview_add_tile(tileview, 0, 0, LV_DIR_HOR);
    tile_claude   = lv_tileview_add_tile(tileview, 1, 0, LV_DIR_HOR);
    tile_settings = lv_tileview_add_tile(tileview, 2, 0, LV_DIR_HOR);

    init_home_tile(tile_home);
    init_claude_tile(tile_claude);
    init_settings_tile(tile_settings);

    lv_obj_add_event_cb(tileview, tileview_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

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
    uint32_t col = (screen == SCREEN_CLAUDE) ? 1 : (screen == SCREEN_SETTINGS) ? 2 : 0;
    lv_tileview_set_tile_by_index(tileview, col, 0, LV_ANIM_OFF);
    current_screen = screen;
    if (screen == SCREEN_SETTINGS) ui_refresh_settings();
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
