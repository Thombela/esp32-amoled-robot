#pragma once
#include <lvgl.h>

// WiFi network list: scans on open, lists nearby SSIDs, tap a row to open
// the credential screen (wifi_credentials_screen_*) for that network.
void wifi_setup_screen_init(lv_obj_t* tile);
void wifi_setup_screen_open(void);  // called by ui_open_wifi_setup(): triggers a fresh scan

// WiFi credential entry: username (enterprise only) + password + Connect/Cancel.
void wifi_credentials_screen_init(lv_obj_t* tile);
void wifi_credentials_screen_open(const char* ssid, bool secured, bool enterprise);

// Drives the async scan-result poll and the post-Connect status poll; a
// cheap no-op when neither is in flight. Called unconditionally from
// ui_tick() in ui/ui.cpp.
void wifi_setup_tick(void);
