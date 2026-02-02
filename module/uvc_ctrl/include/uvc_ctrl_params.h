#ifndef UVC_CTRL_PARAMS_H
#define UVC_CTRL_PARAMS_H

#include <stddef.h>
#include "uvc_ctrl_registry.h"

#define UVC_ENTITY_ID_CAMERA_TERMINAL   0x01
#define UVC_ENTITY_ID_PROCESSING_UNIT  0x02

#define UVC_CTRL_PARAM_LIST(X) \
    X(UVC_ENTITY_ID_PROCESSING_UNIT, 0x02, "Brightness", 2, 0, 0, 255, 1, 128, brightness), \
    X(UVC_ENTITY_ID_PROCESSING_UNIT, 0x03, "Contrast", 2, 0, 0, 255, 1, 0, contrast), \
    X(UVC_ENTITY_ID_PROCESSING_UNIT, 0x06, "Hue", 2, 0, 0, 255, 1, 0, hue), \
    X(UVC_ENTITY_ID_PROCESSING_UNIT, 0x07, "Saturation", 2, 0, 0, 255, 1, 0, saturation), \
    X(UVC_ENTITY_ID_PROCESSING_UNIT, 0x0b, "WhiteBalanceTempAuto", 1, 0, 0, 1, 1, 0, white_balance_temp_auto), \
    X(UVC_ENTITY_ID_PROCESSING_UNIT, 0x10, "HueAuto", 1, 0, 0, 1, 1, 0, hue_auto)

#define UVC_CTRL_PARAM_DECL_LIST(X) \
    X(UVC_ENTITY_ID_PROCESSING_UNIT, 0x02, "Brightness", 2, 0, 0, 255, 1, 128, brightness); \
    X(UVC_ENTITY_ID_PROCESSING_UNIT, 0x03, "Contrast", 2, 0, 0, 255, 1, 0, contrast); \
    X(UVC_ENTITY_ID_PROCESSING_UNIT, 0x06, "Hue", 2, 0, 0, 255, 1, 0, hue); \
    X(UVC_ENTITY_ID_PROCESSING_UNIT, 0x07, "Saturation", 2, 0, 0, 255, 1, 0, saturation); \
    X(UVC_ENTITY_ID_PROCESSING_UNIT, 0x0b, "WhiteBalanceTempAuto", 1, 0, 0, 1, 1, 0, white_balance_temp_auto); \
    X(UVC_ENTITY_ID_PROCESSING_UNIT, 0x10, "HueAuto", 1, 0, 0, 1, 1, 0, hue_auto)

extern const uvc_ctrl_entry_t g_uvc_ctrl_entries[];
extern const size_t g_uvc_ctrl_entry_count;

#endif
