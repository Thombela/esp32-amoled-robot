#include "wifi_setup.h"
#include "wifi_poll.h"
#include "../../../ui/ui.h"

#include <Arduino.h>
#include <WiFi.h>

// ======== Network list: scans on open, lists nearby SSIDs ========

struct ScanEntry {
    char ssid[33];
    int32_t rssi;
    wifi_auth_mode_t auth;
};
#define MAX_SCAN_ENTRIES 24
static ScanEntry scan_entries[MAX_SCAN_ENTRIES];
static int scan_count = 0;
static bool scanning = false;

static lv_obj_t* wifi_list;
static lv_obj_t* list_status_label;

static bool is_enterprise_auth(wifi_auth_mode_t auth) {
    return auth == WIFI_AUTH_WPA2_ENTERPRISE || auth == WIFI_AUTH_WPA3_ENTERPRISE ||
           auth == WIFI_AUTH_WPA2_WPA3_ENTERPRISE || auth == WIFI_AUTH_WPA_ENTERPRISE ||
           auth == WIFI_AUTH_WPA3_ENT_192;
}

static const char* auth_tag(wifi_auth_mode_t auth) {
    if (auth == WIFI_AUTH_OPEN) return "Open";
    if (is_enterprise_auth(auth)) return "Enterprise";
    if (auth == WIFI_AUTH_WEP) return "WEP";
    if (auth == WIFI_AUTH_WPA3_PSK || auth == WIFI_AUTH_WPA2_WPA3_PSK) return "WPA3";
    return "WPA2";
}

static void row_clicked_cb(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= scan_count) return;
    const ScanEntry& entry = scan_entries[idx];
    bool secured = entry.auth != WIFI_AUTH_OPEN;
    ui_open_wifi_credentials(entry.ssid, secured, is_enterprise_auth(entry.auth));
}

static void populate_list(void) {
    lv_obj_clean(wifi_list);
    lv_obj_set_style_pad_row(wifi_list, 8, 0);
    char buf[48];
    for (int i = 0; i < scan_count; i++) {
        snprintf(buf, sizeof(buf), "%s   (%s)", scan_entries[i].ssid, auth_tag(scan_entries[i].auth));
        lv_obj_t* btn = lv_list_add_button(wifi_list, NULL, buf);
        // No LVGL theme is applied anywhere in this app (every widget styles
        // itself explicitly, same as ui_make_panel) — without these, a list
        // button falls back to a transparent background with black text,
        // invisible against the black screen.
        lv_obj_set_style_bg_color(btn, COL_PANEL, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_style_text_font(btn, L.wifi_row_font, 0);
        lv_obj_set_style_text_color(btn, COL_TEXT, 0);
        // Rows are clickable by default, which would otherwise swallow a
        // swipe-left starting on one — bubble it up to the overlay's own
        // back-gesture handler (same reasoning as ui_make_panel's rows).
        lv_obj_add_flag(btn, LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_add_event_cb(btn, row_clicked_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }
}

// Dedupe by SSID (keep the strongest RSSI instance), sort by RSSI descending,
// skip hidden (empty) SSIDs — matches typical phone WiFi-list UX.
static void handle_scan_results(int n) {
    scan_count = 0;
    for (int i = 0; i < n && scan_count < MAX_SCAN_ENTRIES; i++) {
        String ssid = WiFi.SSID(i);
        if (ssid.length() == 0) continue;
        int32_t rssi = WiFi.RSSI(i);
        wifi_auth_mode_t auth = WiFi.encryptionType(i);

        int existing = -1;
        for (int j = 0; j < scan_count; j++) {
            if (strcmp(scan_entries[j].ssid, ssid.c_str()) == 0) { existing = j; break; }
        }
        if (existing >= 0) {
            if (rssi > scan_entries[existing].rssi) {
                scan_entries[existing].rssi = rssi;
                scan_entries[existing].auth = auth;
            }
            continue;
        }
        strlcpy(scan_entries[scan_count].ssid, ssid.c_str(), sizeof(scan_entries[scan_count].ssid));
        scan_entries[scan_count].rssi = rssi;
        scan_entries[scan_count].auth = auth;
        scan_count++;
    }
    // Insertion sort descending by RSSI — scan_count is small (<= 24).
    for (int i = 1; i < scan_count; i++) {
        ScanEntry key = scan_entries[i];
        int j = i - 1;
        while (j >= 0 && scan_entries[j].rssi < key.rssi) {
            scan_entries[j + 1] = scan_entries[j];
            j--;
        }
        scan_entries[j + 1] = key;
    }
    WiFi.scanDelete();
    populate_list();
}

void wifi_setup_screen_init(lv_obj_t* tile) {
    lv_obj_set_style_bg_color(tile, COL_BG, 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tile, 0, 0);
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(tile);
    lv_label_set_text(title, "Wi-Fi");
    lv_obj_set_style_text_font(title, L.title_font, 0);
    lv_obj_set_style_text_color(title, COL_TEXT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, L.title_y);

    list_status_label = lv_label_create(tile);
    lv_label_set_text(list_status_label, "Scanning...");
    lv_obj_set_style_text_font(list_status_label, L.wifi_row_font, 0);
    lv_obj_set_style_text_color(list_status_label, COL_DIM, 0);
    lv_obj_align(list_status_label, LV_ALIGN_TOP_MID, 0, L.wifi_list_y - 24);

    // Content-area height mirrors ui.cpp's own zone_h math (scr_h/20 reserved
    // for the home-indicator bar below every app overlay) — computed from L
    // rather than lv_obj_get_height(tile), which would read back stale
    // coords from before LVGL's next layout pass resolves the size that was
    // just set on `tile` a few lines up in ui_init().
    int16_t content_h = L.scr_h - L.scr_h / 20;
    wifi_list = lv_list_create(tile);
    lv_obj_set_pos(wifi_list, L.margin, L.wifi_list_y);
    lv_obj_set_size(wifi_list, L.content_w, content_h - L.wifi_list_y - L.margin);
    lv_obj_set_style_bg_opa(wifi_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wifi_list, 0, 0);
    lv_obj_set_scroll_dir(wifi_list, LV_DIR_VER);
}

void wifi_setup_screen_open(void) {
    scan_count = 0;
    lv_obj_clean(wifi_list);
    lv_label_set_text(list_status_label, "Scanning...");
    scanning = true;
    // A previous connect attempt (failed or otherwise) can leave the radio
    // mid-retry against an AP, which blocks a fresh scan (WIFI_SCAN_FAILED).
    // Force a clean STA state before every scan so opening this screen
    // always works regardless of what happened last time.
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.scanNetworks(true /*async*/, false /*show_hidden*/);
}

// ======== Credential entry: username (enterprise only) + password ========

static lv_obj_t* cred_title;
static lv_obj_t* cred_username_ta;
static lv_obj_t* cred_ttls_switch;
static lv_obj_t* cred_ttls_label;
static lv_obj_t* cred_password_ta;
static lv_obj_t* cred_show_switch;
static lv_obj_t* cred_show_label;
static lv_obj_t* cred_status_label;
static lv_obj_t* cred_keyboard;
static lv_obj_t* cred_connect_btn;
static lv_obj_t* cred_cancel_btn;
static char cred_ssid[33];
static bool cred_enterprise = false;

static bool connecting = false;
static uint32_t connect_started_ms = 0;
#define CONNECT_TIMEOUT_MS 15000

static lv_obj_t* make_text_button(lv_obj_t* parent, int x, int y, int w, int h,
                                   const char* text, lv_color_t bg, lv_event_cb_t cb) {
    lv_obj_t* btn = lv_obj_create(parent);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, COL_TEXT, 0);
    lv_obj_center(label);
    return btn;
}

static void ta_focus_cb(lv_event_t* e) {
    lv_obj_t* ta = (lv_obj_t*)lv_event_get_target(e);
    lv_keyboard_set_textarea(cred_keyboard, ta);
    lv_obj_clear_flag(cred_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(cred_keyboard);
}

static void kb_ready_cancel_cb(lv_event_t* e) {
    (void)e;
    lv_obj_add_flag(cred_keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void show_switch_cb(lv_event_t* e) {
    (void)e;
    bool checked = lv_obj_has_state(cred_show_switch, LV_STATE_CHECKED);
    lv_textarea_set_password_mode(cred_password_ta, !checked);
}

static void connect_clicked_cb(lv_event_t* e) {
    (void)e;
    const char* username = lv_textarea_get_text(cred_username_ta);
    const char* password = lv_textarea_get_text(cred_password_ta);
    bool use_ttls = lv_obj_has_state(cred_ttls_switch, LV_STATE_CHECKED);
    wifi_poll_connect(cred_ssid, username, password, cred_enterprise, use_ttls);

    lv_obj_add_flag(cred_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_color(cred_status_label, COL_DIM, 0);
    lv_label_set_text(cred_status_label, "Connecting...");
    connecting = true;
    connect_started_ms = millis();
}

static void cancel_clicked_cb(lv_event_t* e) {
    (void)e;
    ui_close_wifi_credentials();
}

void wifi_credentials_screen_init(lv_obj_t* tile) {
    lv_obj_set_style_bg_color(tile, COL_BG, 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tile, 0, 0);
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);

    cred_title = lv_label_create(tile);
    lv_obj_set_style_text_font(cred_title, L.wifi_field_font, 0);
    lv_obj_set_style_text_color(cred_title, COL_TEXT, 0);
    lv_label_set_long_mode(cred_title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(cred_title, L.content_w);
    lv_obj_set_style_text_align(cred_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(cred_title, LV_ALIGN_TOP_MID, 0, L.title_y);

    // No LVGL theme is applied anywhere in this app (see the same note in
    // wifi_setup.cpp's populate_list()) — textarea/switch/keyboard all need
    // explicit colors or they render invisible (transparent bg, black text)
    // against the black screen.
    cred_username_ta = lv_textarea_create(tile);
    lv_textarea_set_one_line(cred_username_ta, true);
    lv_textarea_set_placeholder_text(cred_username_ta, "Username");
    lv_obj_set_size(cred_username_ta, L.content_w, L.wifi_field_h);
    lv_obj_set_style_bg_color(cred_username_ta, COL_PANEL, 0);
    lv_obj_set_style_bg_opa(cred_username_ta, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cred_username_ta, 0, 0);
    lv_obj_set_style_radius(cred_username_ta, 8, 0);
    lv_obj_set_style_text_color(cred_username_ta, COL_TEXT, 0);
    lv_obj_set_style_text_font(cred_username_ta, L.wifi_field_font, 0);
    lv_obj_add_event_cb(cred_username_ta, ta_focus_cb, LV_EVENT_FOCUSED, NULL);

    // Enterprise-only: 802_1X_AUTH_FAILED looks identical whether the
    // password is wrong or the network just wants TTLS instead of the
    // default PEAP — this lets the user try both without needing to know
    // their network's RADIUS config up front.
    cred_ttls_switch = lv_switch_create(tile);
    lv_obj_set_style_bg_color(cred_ttls_switch, COL_BAR_BG, 0);
    lv_obj_set_style_bg_opa(cred_ttls_switch, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(cred_ttls_switch, COL_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(cred_ttls_switch, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(cred_ttls_switch, COL_TEXT, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(cred_ttls_switch, LV_OPA_COVER, LV_PART_KNOB);

    cred_ttls_label = lv_label_create(tile);
    lv_label_set_text(cred_ttls_label, "Use TTLS (instead of PEAP)");
    lv_obj_set_style_text_font(cred_ttls_label, L.wifi_row_font, 0);
    lv_obj_set_style_text_color(cred_ttls_label, COL_DIM, 0);

    cred_password_ta = lv_textarea_create(tile);
    lv_textarea_set_one_line(cred_password_ta, true);
    lv_textarea_set_password_mode(cred_password_ta, true);
    lv_textarea_set_placeholder_text(cred_password_ta, "Password");
    lv_obj_set_size(cred_password_ta, L.content_w, L.wifi_field_h);
    lv_obj_set_style_bg_color(cred_password_ta, COL_PANEL, 0);
    lv_obj_set_style_bg_opa(cred_password_ta, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cred_password_ta, 0, 0);
    lv_obj_set_style_radius(cred_password_ta, 8, 0);
    lv_obj_set_style_text_color(cred_password_ta, COL_TEXT, 0);
    lv_obj_set_style_text_font(cred_password_ta, L.wifi_field_font, 0);
    lv_obj_add_event_cb(cred_password_ta, ta_focus_cb, LV_EVENT_FOCUSED, NULL);

    cred_show_switch = lv_switch_create(tile);
    lv_obj_set_style_bg_color(cred_show_switch, COL_BAR_BG, 0);
    lv_obj_set_style_bg_opa(cred_show_switch, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(cred_show_switch, COL_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(cred_show_switch, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(cred_show_switch, COL_TEXT, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(cred_show_switch, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_add_event_cb(cred_show_switch, show_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);

    cred_show_label = lv_label_create(tile);
    lv_label_set_text(cred_show_label, "Show password");
    lv_obj_set_style_text_font(cred_show_label, L.wifi_row_font, 0);
    lv_obj_set_style_text_color(cred_show_label, COL_DIM, 0);

    cred_status_label = lv_label_create(tile);
    lv_obj_set_style_text_font(cred_status_label, L.wifi_row_font, 0);
    lv_obj_set_style_text_color(cred_status_label, COL_DIM, 0);
    lv_obj_set_width(cred_status_label, L.content_w);
    lv_obj_set_style_text_align(cred_status_label, LV_TEXT_ALIGN_CENTER, 0);

    int btn_w = (L.content_w - 16) / 2;
    cred_cancel_btn  = make_text_button(tile, L.margin, 0, btn_w, L.wifi_field_h, "Cancel", COL_PANEL, cancel_clicked_cb);
    cred_connect_btn = make_text_button(tile, L.margin + btn_w + 16, 0, btn_w, L.wifi_field_h, "Connect", COL_ACCENT, connect_clicked_cb);

    cred_keyboard = lv_keyboard_create(tile);
    lv_obj_set_size(cred_keyboard, L.scr_w, L.wifi_kb_h);
    lv_obj_align(cred_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(cred_keyboard, COL_BG, 0);
    lv_obj_set_style_bg_opa(cred_keyboard, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cred_keyboard, 0, 0);
    lv_obj_set_style_bg_color(cred_keyboard, COL_PANEL, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(cred_keyboard, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_text_color(cred_keyboard, COL_TEXT, LV_PART_ITEMS);
    lv_obj_set_style_border_width(cred_keyboard, 0, LV_PART_ITEMS);
    lv_obj_set_style_radius(cred_keyboard, 4, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(cred_keyboard, COL_ACCENT, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(cred_keyboard, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_add_event_cb(cred_keyboard, kb_ready_cancel_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(cred_keyboard, kb_ready_cancel_cb, LV_EVENT_CANCEL, NULL);
    lv_obj_add_flag(cred_keyboard, LV_OBJ_FLAG_HIDDEN);
}

void wifi_credentials_screen_open(const char* ssid, bool secured, bool enterprise) {
    strlcpy(cred_ssid, ssid, sizeof(cred_ssid));
    cred_enterprise = enterprise;
    connecting = false;

    lv_label_set_text(cred_title, ssid);
    lv_textarea_set_text(cred_username_ta, "");
    lv_textarea_set_text(cred_password_ta, "");
    lv_textarea_set_password_mode(cred_password_ta, true);
    lv_obj_clear_state(cred_show_switch, LV_STATE_CHECKED);
    lv_obj_clear_state(cred_ttls_switch, LV_STATE_CHECKED);
    lv_obj_add_flag(cred_keyboard, LV_OBJ_FLAG_HIDDEN);

    // Switch rows (TTLS/show-password) are shorter than a text field, so they
    // use a tighter step — otherwise username+TTLS+password+show-password+
    // status+buttons doesn't fit above the keyboard on the large breakpoint.
    int switch_row_h = 40;
    int switch_row_gap = 8;

    int y = L.wifi_fields_y;

    if (enterprise) {
        lv_obj_clear_flag(cred_username_ta, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(cred_username_ta, L.margin, y);
        y += L.wifi_field_h + L.wifi_field_gap;

        lv_obj_clear_flag(cred_ttls_switch, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(cred_ttls_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(cred_ttls_switch, L.margin, y);
        lv_obj_align_to(cred_ttls_label, cred_ttls_switch, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
        y += switch_row_h + switch_row_gap;
    } else {
        lv_obj_add_flag(cred_username_ta, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(cred_ttls_switch, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(cred_ttls_label, LV_OBJ_FLAG_HIDDEN);
    }

    if (secured) {
        lv_obj_clear_flag(cred_password_ta, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(cred_password_ta, L.margin, y);
        y += L.wifi_field_h + L.wifi_field_gap;

        lv_obj_clear_flag(cred_show_switch, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(cred_show_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(cred_show_switch, L.margin, y);
        lv_obj_align_to(cred_show_label, cred_show_switch, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
        y += switch_row_h;

        lv_label_set_text(cred_status_label, "");
    } else {
        lv_obj_add_flag(cred_password_ta, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(cred_show_switch, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(cred_show_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(cred_status_label, "Open network");
    }

    lv_obj_set_style_text_color(cred_status_label, COL_DIM, 0);
    lv_obj_align(cred_status_label, LV_ALIGN_TOP_MID, 0, y);
    y += L.wifi_field_gap + 20;

    int btn_w = (L.content_w - 16) / 2;
    lv_obj_set_pos(cred_cancel_btn, L.margin, y);
    lv_obj_set_pos(cred_connect_btn, L.margin + btn_w + 16, y);
}

// ======== Shared tick: drives the async scan poll + connect-status poll ========

void wifi_setup_tick(void) {
    if (scanning) {
        int n = WiFi.scanComplete();
        if (n == WIFI_SCAN_RUNNING) return;
        scanning = false;
        if (n < 0) {
            lv_label_set_text(list_status_label, "Scan failed");
            return;
        }
        handle_scan_results(n);
        lv_label_set_text_fmt(list_status_label, "%d network%s found", scan_count, scan_count == 1 ? "" : "s");
    }

    if (connecting) {
        if (wifi_poll_is_connected()) {
            connecting = false;
            lv_obj_set_style_text_color(cred_status_label, COL_GREEN, 0);
            lv_label_set_text(cred_status_label, "Connected!");
        } else if (millis() - connect_started_ms > CONNECT_TIMEOUT_MS) {
            connecting = false;
            lv_obj_set_style_text_color(cred_status_label, COL_RED, 0);
            lv_label_set_text(cred_status_label, "Connection failed");
        }
    }
}
