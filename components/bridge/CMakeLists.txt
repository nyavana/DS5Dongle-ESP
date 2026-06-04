# BT <-> USB HID report bridge (ported from the Pico main.cpp relay logic).
# Owns the TinyUSB HID SET/GET-report callbacks, so it pulls in report_buffer,
# bthost, state_mgr, config, cmd, common (utils.h) and esp_tinyusb.
idf_component_register(
    SRCS "bridge.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        report_buffer
        bthost
        state_mgr
        config
        cmd
        common
        espressif__esp_tinyusb
)
