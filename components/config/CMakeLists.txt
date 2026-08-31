# Device configuration (ported from legacy-pico/src/config.cpp), persisted in
# NVS. `common` provides the CRC helpers (utils.h); `nvs_flash` the blob store.
idf_component_register(
    SRCS "config.cpp" "config_contract.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES common nvs_flash esp_driver_gpio
)
