#include "ui.h"
#include <stdint.h>
#include <lvgl.h>
#include "icons/icons.h"
#include "backgrounds/backgrounds.h"
#include "icons/app_icons.h"
#include "../apps/claude/claude.h"
#include "../apps/claude/claude_settings.h"
#include "../apps/settings/settings.h"
#include "../apps/settings/wifi/wifi_setup.h"
#include "../hal/board_caps.h"

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
// breakpoint — never editing the screen-builder functions below. Struct
// definition lives in ui.h so apps/claude and apps/settings can read L too.
Layout L = {};

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

        L.wifi_row_font = &font_styrene_20;
        L.wifi_row_h = 64; L.wifi_list_y = 100;
        L.wifi_field_font = &font_styrene_24;
        L.wifi_field_h = 56; L.wifi_field_gap = 16; L.wifi_fields_y = 110;
        L.wifi_kb_h = 200;
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

        L.wifi_row_font = &font_styrene_14;
        L.wifi_row_h = 52; L.wifi_list_y = 80;
        L.wifi_field_font = &font_styrene_20;
        L.wifi_field_h = 46; L.wifi_field_gap = 12; L.wifi_fields_y = 90;
        L.wifi_kb_h = 180;
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

        L.wifi_row_font = &font_styrene_12;
        L.wifi_row_h = 36; L.wifi_list_y = 40;
        L.wifi_field_font = &font_styrene_14;
        L.wifi_field_h = 32; L.wifi_field_gap = 8; L.wifi_fields_y = 44;
        L.wifi_kb_h = 110;
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
static lv_obj_t* overlay_wifi_setup;
static lv_obj_t* overlay_wifi_credentials;
static lv_obj_t* overlay_claude_settings;
static lv_obj_t* lbl_placeholder_name;

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

lv_obj_t* ui_make_panel(lv_obj_t* parent, int x, int y, int w, int h) {
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
    // The tile itself no longer scrolls — only `content` below does — so the
    // background stays put underneath a scrolling category grid instead of
    // scrolling away with it.
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);

    init_bg_dsc_rgb565(&library_bg_dsc, BG_LIBRARY_W, BG_LIBRARY_H, bg_library_data);
    lv_obj_t* bg = lv_image_create(tile);
    lv_image_set_src(bg, &library_bg_dsc);
    lv_obj_set_pos(bg, 0, 0);
    lv_obj_clear_flag(bg, LV_OBJ_FLAG_CLICKABLE);

    // Scrollable wrapper for everything that should scroll: title + grid.
    // Vertical-only, same reasoning as Settings: a real scrollbar/bounce
    // makes a 6-category (2x3) grid obviously scrollable instead of dead,
    // and horizontal stays free for the tileview's own Home<->Library page.
    lv_obj_t* content = lv_obj_create(tile);
    lv_obj_set_pos(content, 0, 0);
    lv_obj_set_size(content, L.scr_w, L.scr_h);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_add_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);

    lv_obj_t* title = lv_label_create(content);
    lv_label_set_text(title, "Library");
    lv_obj_set_style_text_font(title, L.title_font, 0);
    lv_obj_set_style_text_color(title, COL_TEXT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, L.title_y);

    for (int i = 0; i < CATEGORY_COUNT; i++) {
        int col = i % 2;
        int row = i / 2;
        int x = L.margin + col * (L.lib_card_w + L.lib_card_gap);
        int y = L.lib_card_y + row * L.lib_row_stride;
        lv_obj_t* card = ui_make_panel(content, x, y, L.lib_card_w, L.lib_card_h);
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
        lv_obj_t* label = lv_label_create(content);
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

    popup_card = ui_make_panel(popup_scrim, (L.scr_w - L.popup_w) / 2, (L.scr_h - L.popup_h) / 2,
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

static void hide_all_overlays(void) {
    lv_obj_add_flag(popup_scrim, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(overlay_claude, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(overlay_settings, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(overlay_placeholder, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(overlay_wifi_setup, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(overlay_wifi_credentials, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(overlay_claude_settings, LV_OBJ_FLAG_HIDDEN);
}

static void close_to_library(void) {
    hide_all_overlays();
    lv_tileview_set_tile_by_index(tileview, 1, 0, LV_ANIM_OFF);
    current_screen = SCREEN_LIBRARY;
}

// The home-indicator bar always goes to the Home tile, mirroring iOS's own
// home-indicator (it never means "back one level" — that's the top-left
// back button's job, see make_back_button below).
static void close_to_home(void) {
    hide_all_overlays();
    lv_tileview_set_tile_by_index(tileview, 0, 0, LV_ANIM_OFF);
    current_screen = SCREEN_HOME;
}

// The WiFi setup screen is nested under Settings, not a Library app — its
// own back target is Settings, not Library.
static void close_to_settings(void) {
    hide_all_overlays();
    lv_obj_clear_flag(overlay_settings, LV_OBJ_FLAG_HIDDEN);
    current_screen = SCREEN_SETTINGS;
}

// The credential entry screen is nested under the WiFi network list.
static void close_to_wifi_setup(void) {
    hide_all_overlays();
    lv_obj_clear_flag(overlay_wifi_setup, LV_OBJ_FLAG_HIDDEN);
    current_screen = SCREEN_WIFI_SETUP;
}

// The Claude settings screen is nested under the Claude tile itself.
static void close_to_claude(void) {
    hide_all_overlays();
    lv_obj_clear_flag(overlay_claude, LV_OBJ_FLAG_HIDDEN);
    current_screen = SCREEN_CLAUDE;
}

// Both app screens are full-screen with no chrome of their own: a top-left
// back button (see make_back_button) goes back exactly one level; tapping
// or swiping up the home-indicator bar always jumps to Home, regardless of
// nesting depth — mirroring iOS, where the home indicator never means "back
// one level". Each overlay registers its own "where does back go" target
// (Library for most; Settings for the WiFi list; the WiFi list for
// credential entry) via the event's user_data, rather than hardcoding one
// destination.
static void back_button_clicked_cb(lv_event_t* e) {
    auto close_target = (void (*)(void))lv_event_get_user_data(e);
    close_target();
}

// A small square button in the top-left corner (left of any app-specific
// icons, which are shifted right to make room). Uses a plain "<" glyph
// rather than an icon asset — the custom bitmap fonts only bake ASCII
// 0x20-0x7E, and "<" reads clearly as "back" without needing a new PNG run
// through the icon pipeline.
static void make_back_button(lv_obj_t* parent, void (*close_target)(void)) {
    int size = L.batt_w;
    lv_obj_t* btn = lv_obj_create(parent);
    lv_obj_set_size(btn, size, size);
    lv_obj_set_pos(btn, L.margin, L.batt_y);
    lv_obj_set_style_bg_color(btn, COL_PANEL, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn, back_button_clicked_cb, LV_EVENT_CLICKED, (void*)close_target);

    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, "<");
    lv_obj_set_style_text_color(label, COL_TEXT, 0);
    lv_obj_set_style_text_font(label, L.wifi_row_font, 0);
    lv_obj_center(label);
}

static void home_indicator_cb(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        close_to_home();
        return;
    }
    lv_indev_t* indev = lv_indev_active();
    if (!indev) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    if (dir & (LV_DIR_TOP | LV_DIR_LEFT)) close_to_home();
}

// A centered, 50%-wide bar at the bottom of the screen — tap it, or swipe up
// from it, to jump straight to Home. zone_h (the touch target, and the
// space init_*_tile's content area leaves clear above it) is capped at 5%
// of screen height; the visible bar itself stays a slim iOS-style pill
// within that target rather than filling the whole target with color.
static void make_home_indicator(lv_obj_t* parent, int zone_h) {
    int zone_w = L.scr_w / 2;
    lv_obj_t* zone = lv_obj_create(parent);
    lv_obj_set_style_bg_opa(zone, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(zone, 0, 0);
    lv_obj_set_style_pad_all(zone, 0, 0);
    lv_obj_clear_flag(zone, LV_OBJ_FLAG_SCROLLABLE);
    // Every LVGL object bubbles gestures to its parent by default (the flag
    // is set automatically on creation) — the bubble walk only stops at the
    // first ancestor WITHOUT the flag. Clearing it here makes `zone` itself
    // that stopping point, so a swipe starting on the indicator bar reaches
    // home_indicator_cb directly instead of continuing up to the overlay
    // (and past that, since overlays clear it too — see below — all the way
    // to the screen root, where nothing is listening).
    lv_obj_clear_flag(zone, LV_OBJ_FLAG_GESTURE_BUBBLE);
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
    hide_all_overlays();

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
    int zone_h = L.scr_h / 20;  // touch target, capped at 5% of screen height

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
    claude_screen_init(claude_content);
    make_back_button(overlay_claude, close_to_library);
    make_home_indicator(overlay_claude, zone_h);
    lv_obj_add_flag(overlay_claude, LV_OBJ_FLAG_HIDDEN);

    overlay_claude_settings = lv_obj_create(scr);
    lv_obj_set_pos(overlay_claude_settings, 0, 0);
    lv_obj_set_size(overlay_claude_settings, L.scr_w, L.scr_h);
    lv_obj_set_style_bg_color(overlay_claude_settings, COL_BG, 0);
    lv_obj_set_style_bg_opa(overlay_claude_settings, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(overlay_claude_settings, 0, 0);
    lv_obj_set_style_pad_all(overlay_claude_settings, 0, 0);
    lv_obj_clear_flag(overlay_claude_settings, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* claude_settings_content = lv_obj_create(overlay_claude_settings);
    lv_obj_set_pos(claude_settings_content, 0, 0);
    lv_obj_set_size(claude_settings_content, L.scr_w, L.scr_h - zone_h);
    claude_settings_screen_init(claude_settings_content);
    make_back_button(overlay_claude_settings, close_to_claude);
    make_home_indicator(overlay_claude_settings, zone_h);
    lv_obj_add_flag(overlay_claude_settings, LV_OBJ_FLAG_HIDDEN);

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
    settings_screen_init(settings_content);
    make_back_button(overlay_settings, close_to_library);
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
    make_back_button(overlay_placeholder, close_to_library);
    make_home_indicator(overlay_placeholder, zone_h);
    lv_obj_add_flag(overlay_placeholder, LV_OBJ_FLAG_HIDDEN);

    overlay_wifi_setup = lv_obj_create(scr);
    lv_obj_set_pos(overlay_wifi_setup, 0, 0);
    lv_obj_set_size(overlay_wifi_setup, L.scr_w, L.scr_h);
    lv_obj_set_style_bg_color(overlay_wifi_setup, COL_BG, 0);
    lv_obj_set_style_bg_opa(overlay_wifi_setup, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(overlay_wifi_setup, 0, 0);
    lv_obj_set_style_pad_all(overlay_wifi_setup, 0, 0);
    lv_obj_clear_flag(overlay_wifi_setup, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* wifi_setup_content = lv_obj_create(overlay_wifi_setup);
    lv_obj_set_pos(wifi_setup_content, 0, 0);
    lv_obj_set_size(wifi_setup_content, L.scr_w, L.scr_h - zone_h);
    wifi_setup_screen_init(wifi_setup_content);
    make_back_button(overlay_wifi_setup, close_to_settings);
    make_home_indicator(overlay_wifi_setup, zone_h);
    lv_obj_add_flag(overlay_wifi_setup, LV_OBJ_FLAG_HIDDEN);

    overlay_wifi_credentials = lv_obj_create(scr);
    lv_obj_set_pos(overlay_wifi_credentials, 0, 0);
    lv_obj_set_size(overlay_wifi_credentials, L.scr_w, L.scr_h);
    lv_obj_set_style_bg_color(overlay_wifi_credentials, COL_BG, 0);
    lv_obj_set_style_bg_opa(overlay_wifi_credentials, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(overlay_wifi_credentials, 0, 0);
    lv_obj_set_style_pad_all(overlay_wifi_credentials, 0, 0);
    lv_obj_clear_flag(overlay_wifi_credentials, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* wifi_credentials_content = lv_obj_create(overlay_wifi_credentials);
    lv_obj_set_pos(wifi_credentials_content, 0, 0);
    lv_obj_set_size(wifi_credentials_content, L.scr_w, L.scr_h - zone_h);
    wifi_credentials_screen_init(wifi_credentials_content);
    make_back_button(overlay_wifi_credentials, close_to_wifi_setup);
    make_home_indicator(overlay_wifi_credentials, zone_h);
    lv_obj_add_flag(overlay_wifi_credentials, LV_OBJ_FLAG_HIDDEN);

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
    // UsageData (BLE/wifi_poll's fuller daemon schema) is intentionally
    // unrelated to the Claude tile's daily/weekly % — that's now driven by
    // apps/claude/claude_poll.cpp's own independent {daily, weekly} fetch
    // (see claude_screen_update()), by product decision. This callback stays
    // a no-op unless UsageData grows a use elsewhere.
}

void ui_tick(void) {
    // Drives the WiFi setup screen's async scan poll + post-Connect status
    // poll; a cheap no-op when neither is in flight, so no visibility check
    // is needed here (unlike the Settings refresh below).
    wifi_setup_tick();
    claude_screen_tick();

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
    hide_all_overlays();

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

void ui_refresh_settings(void) {
    settings_screen_refresh();
}

void ui_open_wifi_setup(void) {
    hide_all_overlays();
    lv_obj_clear_flag(overlay_wifi_setup, LV_OBJ_FLAG_HIDDEN);
    current_screen = SCREEN_WIFI_SETUP;
    wifi_setup_screen_open();  // triggers a fresh scan every time it's opened
}

void ui_open_wifi_credentials(const char* ssid, bool secured, bool enterprise) {
    wifi_credentials_screen_open(ssid, secured, enterprise);
    hide_all_overlays();
    lv_obj_clear_flag(overlay_wifi_credentials, LV_OBJ_FLAG_HIDDEN);
    current_screen = SCREEN_WIFI_SETUP;
}

void ui_close_wifi_credentials(void) {
    close_to_wifi_setup();
}

void ui_open_claude_settings(void) {
    claude_settings_screen_open();
    hide_all_overlays();
    lv_obj_clear_flag(overlay_claude_settings, LV_OBJ_FLAG_HIDDEN);
    current_screen = SCREEN_CLAUDE;
}
