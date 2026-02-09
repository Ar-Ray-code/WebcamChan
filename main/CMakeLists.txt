idf_component_register(
    SRCS
        "src/main.c"
        "src/usb_descriptors_override.c"
    INCLUDE_DIRS "include"
    REQUIRES face uvc_ctrl
)

# Override tud_descriptor_configuration_cb to inject a Processing Unit
# into the UVC descriptor topology, and videod_control_xfer_cb to handle
# entity control requests that TinyUSB's video driver does not support.
target_link_libraries(${COMPONENT_LIB} INTERFACE
    "-Wl,--wrap=tud_descriptor_configuration_cb"
    "-Wl,--wrap=videod_control_xfer_cb"
    "-Wl,--undefined=__wrap_tud_descriptor_configuration_cb"
    "-Wl,--undefined=__wrap_videod_control_xfer_cb")
