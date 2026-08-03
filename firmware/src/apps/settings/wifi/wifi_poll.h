#pragma once

// WiFi HTTP usage-data poller — entirely independent of BLE ownership/bonding.
// Board-agnostic (WiFi is identical SoC-level Arduino-ESP32 API on every board
// this project targets), so this lives as flat shared code rather than under
// hal/ or boards/<name>/. Fully opt-in: does nothing unless an SSID has been
// configured via the wifi_ssid/wifi_pass/wifi_url/wifi_secret serial commands
// or the on-device WiFi setup screen (see wifi_setup.h).

void wifi_poll_init(void);

// Poll result handoff to loop() — same lockless volatile-flag + static-buffer
// pattern ble.cpp already uses for its NimBLE-host-task -> loop-task handoff.
bool wifi_poll_has_data(void);
const char* wifi_poll_get_data(void);

// Serial-command setters (main.cpp's check_serial_cmd dispatches into these).
// PSK-only — kept for headless bootstrapping/testing, independent of the
// on-device setup screen below.
void wifi_poll_set_ssid(const char* v);
void wifi_poll_set_pass(const char* v);
void wifi_poll_set_url(const char* v);
void wifi_poll_set_secret(const char* v);
void wifi_poll_print_status(void);

// Save credentials to NVS and (re)connect. Used by the on-device WiFi setup
// screen (apps/settings/wifi/wifi_setup.cpp) — the one path that needs both
// plain WPA2-PSK and WPA2-Enterprise (802.1x, e.g. eduroam/corporate WiFi).
// `username`/`use_ttls` are ignored when `enterprise` is false. Identity is
// always == username (no separate anonymous-identity field). `use_ttls`
// picks EAP-TTLS instead of the default PEAP — both are common; which one a
// given network actually wants isn't discoverable from the credential
// screen alone, so this is a user-flippable toggle rather than a guess.
void wifi_poll_connect(const char* ssid, const char* username, const char* password,
                       bool enterprise, bool use_ttls);

// Status getters for the Settings screen and the WiFi setup screen.
bool wifi_poll_is_connected(void);
bool wifi_poll_is_enterprise(void);
bool wifi_poll_uses_ttls(void);
const char* wifi_poll_get_ssid(void);      // "" if never configured
const char* wifi_poll_get_username(void);  // "" if not enterprise / not set
const char* wifi_poll_get_ip(void);        // "" if not connected
