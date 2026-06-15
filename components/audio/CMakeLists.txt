# Default-disabled audio scaffold (ported from legacy-pico/src/audio.cpp).
# Runtime UAC capture, haptics resampling, and Opus encode remain hardware
# bring-up follow-ups; this component preserves the API and report shape.
idf_component_register(
    SRCS "audio.cpp"
    INCLUDE_DIRS "include"
    REQUIRES
        bthost
        bridge
        config
        state_mgr
)
