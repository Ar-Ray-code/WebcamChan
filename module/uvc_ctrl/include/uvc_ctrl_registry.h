#ifndef UVC_CTRL_REGISTRY_H
#define UVC_CTRL_REGISTRY_H

#include <stddef.h>
#include <stdint.h>

typedef void (*uvc_ctrl_set_cb_t)(const char *name, const uint8_t *data, size_t len);

typedef struct {
    uint8_t entity_id;
    uint8_t control_selector;
    const char *name;
    uint16_t data_len;
    int32_t cur;
    int32_t min;
    int32_t max;
    int32_t res;
    int32_t def;
    volatile int64_t *value_ptr;
    uvc_ctrl_set_cb_t on_set;
} uvc_ctrl_entry_t;

void uvc_ctrl_registry_register(const uvc_ctrl_entry_t *entries, size_t count);

int uvc_ctrl_registry_handle(uint8_t entity_id,
                             uint8_t control_selector,
                             uint8_t request,
                             uint8_t stage,
                             uint8_t *buf,
                             uint16_t len);

#endif
