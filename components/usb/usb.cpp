//
// USB device bring-up (ported from the Pico build's usb.cpp + main.cpp USB
// glue). Uses esp_tinyusb's tinyusb_driver_install (which also creates the USB
// device task), fed the ported DualSense descriptors.
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

// NOTE: the HID class callbacks (tud_hid_get_report_cb / tud_hid_set_report_cb)
// live in the bridge component (group 4), which owns the command / output /
// feature routing. TinyUSB links them by name regardless of component.
