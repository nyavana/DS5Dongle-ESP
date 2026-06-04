# USB device component: esp_tinyusb DualSense emulation (ported from
# legacy-pico/src/usb.cpp + usb_descriptors.cpp). Group 2 = HID-only; the UAC
# audio control callbacks are added in group 6.
idf_component_register(
    SRCS
        "usb.cpp"
        "usb_descriptors.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES
        espressif__esp_tinyusb
)
