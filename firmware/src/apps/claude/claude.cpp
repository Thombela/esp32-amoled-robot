#include "claude.h"
#include "claude_poll.h"
#include "../../ui/ui.h"
#include "../../ui/icons/app_icons.h"

#include <Arduino.h>

static lv_obj_t* lbl_claude_daily_pct;
static lv_obj_t* lbl_claude_daily_reset;
static lv_obj_t* lbl_claude_weekly_pct;
static lv_obj_t* lbl_claude_weekly_reset;
static lv_obj_t* toast_label;
static lv_image_dsc_t gear_icon_dsc;

static bool     toast_visible = false;
static uint32_t toast_hide_at_ms = 0;

static lv_color_t pct_color(float pct) {
    if (pct >= 80.0f) return COL_RED;
    if (pct >= 50.0f) return COL_AMBER;
    return COL_GREEN;
}

// "148" -> "resets in 2h 28m"; "7218" -> "resets in 5d 0h"; "0" -> "".
static void format_reset(int minutes, char* buf, size_t buf_len) {
    if (minutes <= 0) {
        buf[0] = '\0';
    } else if (minutes < 60) {
        snprintf(buf, buf_len, "resets in %dm", minutes);
    } else if (minutes < 24 * 60) {
        snprintf(buf, buf_len, "resets in %dh %dm", minutes / 60, minutes % 60);
    } else {
        snprintf(buf, buf_len, "resets in %dd %dh", minutes / (24 * 60), (minutes / 60) % 24);
    }
}

static void gear_clicked_cb(lv_event_t* e) {
    (void)e;
    ui_open_claude_settings();
}

static void refresh_clicked_cb(lv_event_t* e) {
    (void)e;
    claude_poll_refresh_now();
    claude_screen_show_toast("Refreshing...", false);
}

void claude_screen_init(lv_obj_t* tile) {
    lv_obj_set_style_bg_color(tile, COL_BG, 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tile, 0, 0);
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);

    // Gear icon: opposite corner from the (globally-drawn) battery indicator,
    // opens the data-source settings screen. Reuses the Library's Settings
    // app icon rather than adding a new asset.
    gear_icon_dsc.header.w = APP_ICON_SETTINGS_W;
    gear_icon_dsc.header.h = APP_ICON_SETTINGS_H;
    gear_icon_dsc.header.cf = LV_COLOR_FORMAT_RGB565A8;
    gear_icon_dsc.header.stride = APP_ICON_SETTINGS_W * 2;
    gear_icon_dsc.data = app_icon_settings_data;
    gear_icon_dsc.data_size = APP_ICON_SETTINGS_W * APP_ICON_SETTINGS_H * 3;

    // Shifted right of L.margin to leave room for the shared top-left back
    // button ui.cpp adds to every app overlay (see make_back_button).
    int icons_x0 = L.margin + L.batt_w + 12;

    lv_obj_t* gear = lv_image_create(tile);
    lv_image_set_src(gear, &gear_icon_dsc);
    lv_image_set_scale(gear, 256 * L.batt_w / APP_ICON_SETTINGS_W);
    lv_obj_align(gear, LV_ALIGN_TOP_LEFT, icons_x0, L.batt_y);
    lv_obj_add_flag(gear, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(gear, gear_clicked_cb, LV_EVENT_CLICKED, NULL);

    // "Session" (Anthropic's rolling 5-hour limit) and "Weekly" (7-day limit)
    // — matches the account usage API's own five_hour/seven_day terminology
    // rather than calendar daily/weekly.
    int reset1_y = L.claude_pct1_y + L.claude_pct_font->line_height + 4;
    int reset2_y = L.claude_pct2_y + L.claude_pct_font->line_height + 4;

    lv_obj_t* l1 = lv_label_create(tile);
    lv_label_set_text(l1, "Session");
    lv_obj_set_style_text_font(l1, L.claude_label_font, 0);
    lv_obj_set_style_text_color(l1, COL_DIM, 0);
    lv_obj_align(l1, LV_ALIGN_TOP_MID, 0, L.claude_label1_y);

    lbl_claude_daily_pct = lv_label_create(tile);
    lv_label_set_text(lbl_claude_daily_pct, "--");
    lv_obj_set_style_text_font(lbl_claude_daily_pct, L.claude_pct_font, 0);
    lv_obj_set_style_text_color(lbl_claude_daily_pct, COL_DIM, 0);
    lv_obj_align(lbl_claude_daily_pct, LV_ALIGN_TOP_MID, 0, L.claude_pct1_y);

    lbl_claude_daily_reset = lv_label_create(tile);
    lv_label_set_text(lbl_claude_daily_reset, "");
    lv_obj_set_style_text_font(lbl_claude_daily_reset, L.claude_label_font, 0);
    lv_obj_set_style_text_color(lbl_claude_daily_reset, COL_DIM, 0);
    lv_obj_align(lbl_claude_daily_reset, LV_ALIGN_TOP_MID, 0, reset1_y);

    lv_obj_t* l2 = lv_label_create(tile);
    lv_label_set_text(l2, "Weekly");
    lv_obj_set_style_text_font(l2, L.claude_label_font, 0);
    lv_obj_set_style_text_color(l2, COL_DIM, 0);
    lv_obj_align(l2, LV_ALIGN_TOP_MID, 0, L.claude_label2_y);

    lbl_claude_weekly_pct = lv_label_create(tile);
    lv_label_set_text(lbl_claude_weekly_pct, "--");
    lv_obj_set_style_text_font(lbl_claude_weekly_pct, L.claude_pct_font, 0);
    lv_obj_set_style_text_color(lbl_claude_weekly_pct, COL_DIM, 0);
    lv_obj_align(lbl_claude_weekly_pct, LV_ALIGN_TOP_MID, 0, L.claude_pct2_y);

    lbl_claude_weekly_reset = lv_label_create(tile);
    lv_label_set_text(lbl_claude_weekly_reset, "");
    lv_obj_set_style_text_font(lbl_claude_weekly_reset, L.claude_label_font, 0);
    lv_obj_set_style_text_color(lbl_claude_weekly_reset, COL_DIM, 0);
    lv_obj_align(lbl_claude_weekly_reset, LV_ALIGN_TOP_MID, 0, reset2_y);

    // Small pill next to the gear icon — the bottom of the tile is already
    // fully used by the Session/Weekly blocks plus the toast, with no room
    // left for a second button there.
    int gear_size = L.batt_w;
    lv_obj_t* refresh_btn = lv_obj_create(tile);
    lv_obj_set_size(refresh_btn, 90, gear_size);
    lv_obj_align(refresh_btn, LV_ALIGN_TOP_LEFT, icons_x0 + gear_size + 12, L.batt_y);
    lv_obj_set_style_bg_color(refresh_btn, COL_PANEL, 0);
    lv_obj_set_style_bg_opa(refresh_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(refresh_btn, 8, 0);
    lv_obj_set_style_border_width(refresh_btn, 0, 0);
    lv_obj_clear_flag(refresh_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(refresh_btn, refresh_clicked_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* refresh_label = lv_label_create(refresh_btn);
    lv_label_set_text(refresh_label, "Refresh");
    lv_obj_set_style_text_color(refresh_label, COL_TEXT, 0);
    lv_obj_set_style_text_font(refresh_label, L.wifi_row_font, 0);
    lv_obj_center(refresh_label);

    // Toast: temporary status/error message near the bottom, hidden by
    // default. See claude_screen_show_toast()/claude_screen_tick().
    toast_label = lv_label_create(tile);
    lv_obj_set_style_bg_opa(toast_label, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(toast_label, 8, 0);
    lv_obj_set_style_pad_all(toast_label, 10, 0);
    lv_obj_set_style_text_color(toast_label, COL_TEXT, 0);
    lv_obj_set_style_text_font(toast_label, L.wifi_row_font, 0);
    lv_obj_align(toast_label, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_add_flag(toast_label, LV_OBJ_FLAG_HIDDEN);
}

void claude_screen_update(float session_pct, int session_reset_min,
                           float weekly_pct, int weekly_reset_min) {
    if (!lbl_claude_daily_pct) return;  // tile not built yet

    char buf[24];

    // Round to an int and format with %d rather than %f — this project's
    // LVGL build uses its own lightweight sprintf (LV_USE_STDLIB_SPRINTF
    // defaults to the builtin one, not the libc), and %.0f rendered as the
    // literal text "f%" here instead of substituting the value. %d is
    // universally reliable, so we just do the rounding ourselves.
    lv_label_set_text_fmt(lbl_claude_daily_pct, "%d%%", (int)(session_pct + 0.5f));
    lv_obj_set_style_text_color(lbl_claude_daily_pct, pct_color(session_pct), 0);
    format_reset(session_reset_min, buf, sizeof(buf));
    lv_label_set_text(lbl_claude_daily_reset, buf);

    lv_label_set_text_fmt(lbl_claude_weekly_pct, "%d%%", (int)(weekly_pct + 0.5f));
    lv_obj_set_style_text_color(lbl_claude_weekly_pct, pct_color(weekly_pct), 0);
    format_reset(weekly_reset_min, buf, sizeof(buf));
    lv_label_set_text(lbl_claude_weekly_reset, buf);
}

void claude_screen_show_toast(const char* msg, bool is_error) {
    if (!toast_label) return;
    lv_label_set_text(toast_label, msg);
    lv_obj_set_style_bg_color(toast_label, is_error ? COL_RED : COL_PANEL, 0);
    lv_obj_clear_flag(toast_label, LV_OBJ_FLAG_HIDDEN);
    toast_visible = true;
    toast_hide_at_ms = millis() + 3000;
}

void claude_screen_tick(void) {
    if (toast_visible && (int32_t)(millis() - toast_hide_at_ms) >= 0) {
        lv_obj_add_flag(toast_label, LV_OBJ_FLAG_HIDDEN);
        toast_visible = false;
    }
}
