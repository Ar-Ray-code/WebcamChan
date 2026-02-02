idf_component_register(
    SRCS
        "src/uvc_ctrl_registry.c"
        "src/uvc_ctrl_params.c"
    INCLUDE_DIRS "include"
    REQUIRES tinyusb
)
