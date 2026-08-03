#include "claude_settings.h"
#include "claude_poll.h"
#include "../../ui/ui.h"

static lv_obj_t* url_ta;
static lv_obj_t* status_label;
static lv_obj_t* keyboard;

static void ta_focus_cb(lv_event_t* e) {
    lv_obj_t* ta = (lv_obj_t*)lv_event_get_target(e);
    lv_keyboard_set_textarea(keyboard, ta);
    lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(keyboard);
}

static void kb_ready_cancel_cb(lv_event_t* e) {
    (void)e;
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void set_clicked_cb(lv_event_t* e) {
    (void)e;
    claude_poll_set_url(lv_textarea_get_text(url_ta));
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_color(status_label, COL_GREEN, 0);
    lv_label_set_text(status_label, "Saved");
}

void claude_settings_screen_init(lv_obj_t* tile) {
    lv_obj_set_style_bg_color(tile, COL_BG, 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tile, 0, 0);
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(tile);
    lv_label_set_text(title, "Claude Data Source");
    lv_obj_set_style_text_font(title, L.wifi_field_font, 0);
    lv_obj_set_style_text_color(title, COL_TEXT, 0);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(title, L.content_w);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, L.title_y);

    // No LVGL theme is applied anywhere in this app (see the same note in
    // wifi_setup.cpp) — textarea/button/keyboard all need explicit colors.
    url_ta = lv_textarea_create(tile);
    lv_textarea_set_one_line(url_ta, true);
    lv_textarea_set_placeholder_text(url_ta, "http://host:port/usage");
    lv_obj_set_size(url_ta, L.content_w, L.wifi_field_h);
    lv_obj_set_pos(url_ta, L.margin, L.wifi_fields_y);
    lv_obj_set_style_bg_color(url_ta, COL_PANEL, 0);
    lv_obj_set_style_bg_opa(url_ta, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(url_ta, 0, 0);
    lv_obj_set_style_radius(url_ta, 8, 0);
    lv_obj_set_style_text_color(url_ta, COL_TEXT, 0);
    lv_obj_set_style_text_font(url_ta, L.wifi_field_font, 0);
    lv_obj_add_event_cb(url_ta, ta_focus_cb, LV_EVENT_FOCUSED, NULL);

    int status_y = L.wifi_fields_y + L.wifi_field_h + L.wifi_field_gap;
    status_label = lv_label_create(tile);
    lv_obj_set_style_text_font(status_label, L.wifi_row_font, 0);
    lv_obj_set_style_text_color(status_label, COL_DIM, 0);
    lv_obj_set_width(status_label, L.content_w);
    lv_obj_set_style_text_align(status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, status_y);

    lv_obj_t* set_btn = lv_obj_create(tile);
    lv_obj_set_size(set_btn, L.content_w, L.wifi_field_h);
    lv_obj_set_pos(set_btn, L.margin, status_y + L.wifi_field_gap + 20);
    lv_obj_set_style_bg_color(set_btn, COL_ACCENT, 0);
    lv_obj_set_style_bg_opa(set_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(set_btn, 8, 0);
    lv_obj_set_style_border_width(set_btn, 0, 0);
    lv_obj_clear_flag(set_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(set_btn, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(set_btn, set_clicked_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* set_label = lv_label_create(set_btn);
    lv_label_set_text(set_label, "Set");
    lv_obj_set_style_text_color(set_label, COL_TEXT, 0);
    lv_obj_center(set_label);

    keyboard = lv_keyboard_create(tile);
    lv_obj_set_size(keyboard, L.scr_w, L.wifi_kb_h);
    lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(keyboard, COL_BG, 0);
    lv_obj_set_style_bg_opa(keyboard, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(keyboard, 0, 0);
    lv_obj_set_style_bg_color(keyboard, COL_PANEL, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(keyboard, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_text_color(keyboard, COL_TEXT, LV_PART_ITEMS);
    lv_obj_set_style_border_width(keyboard, 0, LV_PART_ITEMS);
    lv_obj_set_style_radius(keyboard, 4, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(keyboard, COL_ACCENT, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(keyboard, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_add_event_cb(keyboard, kb_ready_cancel_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(keyboard, kb_ready_cancel_cb, LV_EVENT_CANCEL, NULL);
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
}

void claude_settings_screen_open(void) {
    lv_textarea_set_text(url_ta, claude_poll_get_url());
    lv_label_set_text(status_label, "");
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
}
