# Shared BT-input -> USB-input report buffer, guarded by a portMUX_TYPE
# spinlock. Consumed by the report bridge (group 4) and battery indicator
# (group 7). Kept as its own component so main/ stays thin.
idf_component_register(
    SRCS "report_buffer.cpp"
    INCLUDE_DIRS "include"
)
