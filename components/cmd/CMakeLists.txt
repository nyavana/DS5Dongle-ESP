# Custom 0xf6-0xf9 feature-report command interface. Talks to bthost, config,
# battery_led, and usb only through their public component seams.
idf_component_register(
    SRCS "cmd.cpp" "cmd_contract.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES bthost config battery_led usb
)
