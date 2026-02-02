#include "uvc_ctrl_registry.h"
#include "tusb.h"
#include "class/video/video.h"

static const uvc_ctrl_entry_t *s_entries = NULL;
static size_t s_entry_count = 0;
static void (*s_logger)(const char *name) = NULL;

void uvc_ctrl_registry_register(const uvc_ctrl_entry_t *entries, size_t count)
{
    s_entries = entries;
    s_entry_count = count;
}


static const uvc_ctrl_entry_t *find_entry(uint8_t entity_id, uint8_t control_selector)
{
    for (size_t i = 0; i < s_entry_count; i++) {
        const uvc_ctrl_entry_t *entry = &s_entries[i];
        if (entry->entity_id == entity_id && entry->control_selector == control_selector) {
            return entry;
        }
    }
    return NULL;
}

static void write_value_le(uint8_t *buf, uint16_t len, int32_t value)
{
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = (uint8_t)((value >> (8 * i)) & 0xFF);
    }
}

int uvc_ctrl_registry_handle(uint8_t entity_id,
                             uint8_t control_selector,
                             uint8_t request,
                             uint8_t stage,
                             uint8_t *buf,
                             uint16_t len)
{
    const uvc_ctrl_entry_t *entry = find_entry(entity_id, control_selector);
    if (!entry) {
        return VIDEO_ERROR_INVALID_REQUEST;
    }

    switch (request) {
    case VIDEO_REQUEST_GET_INFO:
        if (len >= 1) {
            // Support GET/SET
            buf[0] = 0x03;
        }
        return VIDEO_ERROR_NONE;
    case VIDEO_REQUEST_GET_LEN:
        if (len >= 2) {
            buf[0] = (uint8_t)(entry->data_len & 0xFF);
            buf[1] = (uint8_t)((entry->data_len >> 8) & 0xFF);
        }
        return VIDEO_ERROR_NONE;
    case VIDEO_REQUEST_GET_CUR:
        write_value_le(buf, len, entry->cur);
        return VIDEO_ERROR_NONE;
    case VIDEO_REQUEST_GET_MIN:
        write_value_le(buf, len, entry->min);
        return VIDEO_ERROR_NONE;
    case VIDEO_REQUEST_GET_MAX:
        write_value_le(buf, len, entry->max);
        return VIDEO_ERROR_NONE;
    case VIDEO_REQUEST_GET_RES:
        write_value_le(buf, len, entry->res);
        return VIDEO_ERROR_NONE;
    case VIDEO_REQUEST_GET_DEF:
        write_value_le(buf, len, entry->def);
        return VIDEO_ERROR_NONE;
    case VIDEO_REQUEST_SET_CUR:
        if (stage == CONTROL_STAGE_DATA) {
            if (entry->on_set) {
                entry->on_set(entry->name, buf, len);
            }
            if (s_logger) {
                s_logger(entry->name);
            }
        }
        return VIDEO_ERROR_NONE;
    default:
        return VIDEO_ERROR_INVALID_REQUEST;
    }
}
