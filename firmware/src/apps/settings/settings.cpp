#include "settings.h"
#include "../../ui/ui.h"
#include "brightness/brightness.h"
#include "wifi/wifi_poll.h"
#include "bluetooth/bluetooth_status.h"

static lv_obj_t* lbl_settings_brightness_value;
static lv_obj_t* lbl_settings_ble_value;
static lv_obj_t* lbl_settings_wifi_value;

static void wifi_row_clicked_cb(lv_event_t* e) {
    (void)e;
    ui_open_wifi_setup();
}

void settings_screen_init(lv_obj_t* tile) {
    lv_obj_set_style_bg_color(tile, COL_BG, 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tile, 0, 0);
    lv_obj_set_style_pad_all(tile, 0, 0);
    // Vertical-only: a real scrollbar + bounce-at-the-end feedback makes it
    // obvious there are exactly 3 rows and nothing is stuck, instead of a
    // dead, unresponsive screen. Horizontal stays free for the swipe-left
    // "back to Library" gesture (see back_gesture_cb in ui.cpp).
    lv_obj_add_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(tile, LV_DIR_VER);

    lv_obj_t* title = lv_label_create(tile);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_font(title, L.title_font, 0);
    lv_obj_set_style_text_color(title, COL_TEXT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, L.title_y);

    int y = L.settings_row1_y;

    lv_obj_t* p1 = ui_make_panel(tile, L.margin, y, L.content_w, L.settings_row_h);
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

    lv_obj_t* p2 = ui_make_panel(tile, L.margin, y, L.content_w, L.settings_row_h);
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

    lv_obj_t* p3 = ui_make_panel(tile, L.margin, y, L.content_w, L.settings_row_h);
    lv_obj_t* k3 = lv_label_create(p3);
    lv_label_set_text(k3, "WI-FI  ›");  // hint that this row opens the WiFi setup screen
    lv_obj_set_style_text_font(k3, L.settings_key_font, 0);
    lv_obj_set_style_text_color(k3, COL_DIM, 0);
    lv_obj_set_pos(k3, 0, 0);
    lbl_settings_wifi_value = lv_label_create(p3);
    lv_obj_set_style_text_font(lbl_settings_wifi_value, L.settings_value_font, 0);
    lv_obj_set_style_text_color(lbl_settings_wifi_value, COL_TEXT, 0);
    lv_obj_set_pos(lbl_settings_wifi_value, 0, 30);
    lv_obj_add_event_cb(p3, wifi_row_clicked_cb, LV_EVENT_CLICKED, NULL);

    settings_screen_refresh();
}

void settings_screen_refresh(void) {
    if (!lbl_settings_brightness_value) return;  // tile not built yet

    int pct = brightness_get() * 100 / 255;
    lv_label_set_text_fmt(lbl_settings_brightness_value, "%d%%", pct);

    bluetooth_status_refresh(lbl_settings_ble_value);

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
