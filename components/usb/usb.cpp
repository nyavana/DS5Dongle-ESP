//
// USB device bring-up (ported from the Pico build's usb.cpp + main.cpp USB
// glue). Replaces board_init / tusb_init with esp_tinyusb's
// tinyusb_driver_install (which also creates the USB device task), fed the
// ported DualSense descriptors.
//
// Group 2 scope: HID-only enumeration. The HID GET/SET-report callbacks route
// here as stubs; groups 4-5 fill the command (0xf6-0xf9), output (0x02) and
// feature-store branches. The UAC audio control callbacks land in group 6.
//

#include "usb.h"

#include "esp_log.h"
#include "tinyusb.h"
#include "tusb.h"

// UAC mute/volume state — speaker idx 0 (0x02), mic idx 1 (0x05). Used by the
// audio control callbacks (group 6); defined here to honour usb.h's contract.
uint8_t mute[2] = {0, 0};
float volume[2] = {-100.0f, 0.0f};

// Descriptor accessors (usb_descriptors.cpp).
void usb_descriptors_prepare();
const tusb_desc_device_t *usb_device_descriptor();
const uint8_t *usb_configuration_descriptor();
const char **usb_string_descriptors(int *count);

static const char *TAG = "usb";

void usb_init() {
    usb_descriptors_prepare();

    int str_count = 0;
    const char **strs = usb_string_descriptors(&str_count);

    tinyusb_config_t cfg = {};
    cfg.device_descriptor = usb_device_descriptor();
    cfg.string_descriptor = strs;
    cfg.string_descriptor_count = str_count;
    cfg.external_phy = false;
    cfg.configuration_descriptor = usb_configuration_descriptor();
    cfg.self_powered = false;

    ESP_ERROR_CHECK(tinyusb_driver_install(&cfg));
    ESP_LOGI(TAG, "esp_tinyusb installed: DualSense HID device (VID %04x PID %04x)",
             usb_device_descriptor()->idVendor, usb_device_descriptor()->idProduct);
}

//--------------------------------------------------------------------+
// HID class callbacks (stub routing — filled by groups 4-5)
//--------------------------------------------------------------------+

// GET_REPORT (feature): 0xf6-0xf9 -> device-config command interface; other
// feature IDs -> stored feature data. Returns length, or 0 to STALL.
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type,
                               uint8_t *buffer, uint16_t reqlen) {
    (void) itf;
    (void) report_type;
    (void) buffer;
    (void) reqlen;
    (void) report_id;
    // TODO(group 4/5): if (is_pico_cmd(report_id)) return pico_cmd_get(...);
    //                  else return stored get_feature_data(report_id, reqlen).
    return 0;  // STALL until the cmd / feature-data paths are ported
}

// SET_REPORT / interrupt-OUT: 0xf6-0xf9 -> command interface; report_id 0 with
// leading byte 0x02 -> output state + BT output report; feature IDs
// 0x80/0x60/0x61/0x62 -> feature store.
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t bufsize) {
    (void) itf;
    (void) report_type;
    (void) buffer;
    (void) bufsize;
    (void) report_id;
    // TODO(group 4/5): is_pico_cmd -> pico_cmd_set; report_id 0 / buffer[0]==0x02
    //                  -> state_update + bt_write; feature IDs -> set_feature_data.
}
