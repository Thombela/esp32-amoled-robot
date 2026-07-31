#pragma once

// Single shared entry point for "a usage JSON string arrived" — used by both
// the BLE RX path (ble.cpp) and the WiFi poll path (wifi_poll.cpp), so the
// rate-tracking/chime/UI bookkeeping only lives in one place.
bool usage_apply_json(const char* json);
