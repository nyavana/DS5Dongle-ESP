# App entry component. As modules are ported out of legacy-pico/src they land
# in ../components/<name>/ (one component per subsystem) and are added to
# REQUIRES below — keep `main` thin.
idf_component_register(
    SRCS
        "main.cpp"
    INCLUDE_DIRS
        "."
    REQUIRES
        report_buffer   # shared portMUX-guarded input-report buffer (group 1)
        nvs_flash       # config + BT bond persistence (group 5 / Bluedroid)
        usb             # esp_tinyusb DualSense device (group 2)
        bthost          # Bluedroid BR/EDR HID host (group 3; bt.h)
        config          # NVS-backed Config (group 5 core; config.h)
        bridge          # BT <-> USB HID report relay (group 4; bridge.h)
        audio           # default-disabled audio scaffold (group 6; audio.h)
        battery_led     # plain GPIO low-battery indicator (group 7; battery_led.h)
)
