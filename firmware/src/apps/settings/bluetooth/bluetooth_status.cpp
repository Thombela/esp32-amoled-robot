#include "bluetooth_status.h"
#include "../../../ui/ui.h"
#include "../../../core/ble/ble.h"

void bluetooth_status_refresh(lv_obj_t* lbl_value) {
    ble_state_t bs = ble_get_state();
    lv_obj_set_style_text_color(lbl_value,
        bs == BLE_STATE_CONNECTED ? COL_GREEN : COL_DIM, 0);
    if (bs == BLE_STATE_CONNECTED) {
        const char* name = ble_get_device_name();
        lv_label_set_text_fmt(lbl_value, "Connected\n%s",
            name[0] ? name : ble_get_mac_address());
    } else if (bs == BLE_STATE_ADVERTISING) {
        lv_label_set_text(lbl_value, "Advertising");
    } else if (bs == BLE_STATE_DISCONNECTED) {
        lv_label_set_text(lbl_value, "Disconnected");
    } else {
        lv_label_set_text(lbl_value, "Initializing");
    }
}
