# Custom 0xf6-0xf9 feature-report command interface (ported from
# legacy-pico/src/cmd.cpp). Talks to bthost (RSSI), config (get/set/save), and
# esp_tinyusb (tud_disconnect/connect).
idf_component_register(
    SRCS "cmd.cpp"
    INCLUDE_DIRS "include"
    REQUIRES bthost config espressif__esp_tinyusb
)
