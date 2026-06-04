# Output-state manager (ported as-is from legacy-pico/src/state_mgr.cpp).
# `common` provides the SetStateData layout (utils.h).
idf_component_register(
    SRCS "state_mgr.cpp"
    INCLUDE_DIRS "include"
    REQUIRES common
)
