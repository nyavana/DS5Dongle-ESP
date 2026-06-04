# BR/EDR HID host (ported from legacy-pico/src/bt.cpp) on Bluedroid esp_hidh.
#
# NOTE: named `bthost`, not `bt` — a project component named `bt` would override
# ESP-IDF's own Bluetooth component. The public header is still bt.h.
#
# REQUIRES: `bt` = IDF Bluedroid (esp_bt_*, esp_gap_bt), `esp_hid` = esp_hidh
# host, `common` = ported utils.h, `esp_event` = default event loop.
idf_component_register(
    SRCS "bt.cpp"
    INCLUDE_DIRS "include"
    REQUIRES bt esp_hid common esp_event
)
