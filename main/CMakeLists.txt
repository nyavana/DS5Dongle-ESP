# App entry component. As modules are ported out of legacy-pico/src they should
# land in ../components/<name>/ (one component per subsystem) and be added to
# REQUIRES below — keep `main` thin.
idf_component_register(
    SRCS
        "main.cpp"
    INCLUDE_DIRS
        "."
    REQUIRES
        # bt usb audio config state_mgr   # add component names as they are ported
)
