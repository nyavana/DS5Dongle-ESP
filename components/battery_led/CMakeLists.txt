# Low-battery LED indicator. Uses a plain ESP-IDF GPIO path with the pin
# disabled by default until real ESP32-S31 board hardware identifies it.
idf_component_register(
    SRCS "battery_led.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        config
        report_buffer
        esp_driver_gpio
        esp_timer
)
