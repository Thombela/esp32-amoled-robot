#pragma once

// WiFi HTTP usage-data poller — entirely independent of BLE ownership/bonding.
// Board-agnostic (WiFi is identical SoC-level Arduino-ESP32 API on every board
// this project targets), so this lives as flat shared code rather than under
// hal/ or boards/<name>/. Fully opt-in: does nothing unless an SSID has been
// configured via the wifi_ssid/wifi_pass/wifi_url/wifi_secret serial commands.

void wifi_poll_init(void);

// Poll result handoff to loop() — same lockless volatile-flag + static-buffer
// pattern ble.cpp already uses for its NimBLE-host-task -> loop-task handoff.
bool wifi_poll_has_data(void);
const char* wifi_poll_get_data(void);

// Serial-command setters (main.cpp's check_serial_cmd dispatches into these).
void wifi_poll_set_ssid(const char* v);
void wifi_poll_set_pass(const char* v);
void wifi_poll_set_url(const char* v);
void wifi_poll_set_secret(const char* v);
void wifi_poll_print_status(void);

// Status getters for the Settings screen.
bool wifi_poll_is_connected(void);
const char* wifi_poll_get_ssid(void);   // "" if never configured
const char* wifi_poll_get_ip(void);     // "" if not connected
