#ifndef UVC_CTRL_STATE_H
#define UVC_CTRL_STATE_H

#include <stdint.h>

typedef struct {
    volatile int64_t brightness;
    volatile int64_t contrast;
    volatile int64_t hue;
    volatile int64_t saturation;
    volatile int64_t white_balance_temp_auto;
    volatile int64_t hue_auto;
} uvc_ctrl_state_t;

extern volatile uvc_ctrl_state_t g_uvc_ctrl_state;

typedef void (*uvc_ctrl_value_cb_t)(const char *name, int64_t value);
void uvc_ctrl_state_set_callback(uvc_ctrl_value_cb_t cb);

#endif
