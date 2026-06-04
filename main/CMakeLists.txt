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
        # usb bt bridge config audio battery_led   # added as each is ported
)
