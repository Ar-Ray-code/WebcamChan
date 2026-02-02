#include "uvc_ctrl_params.h"
#include "uvc_ctrl_state.h"

static uvc_ctrl_value_cb_t s_value_cb = NULL;

volatile uvc_ctrl_state_t g_uvc_ctrl_state = {
    .brightness = 128,
    .contrast = 0,
    .hue = 0,
    .saturation = 0,
    .white_balance_temp_auto = 0,
    .hue_auto = 0,
};

void uvc_ctrl_state_set_callback(uvc_ctrl_value_cb_t cb)
{
    s_value_cb = cb;
}

static int64_t read_value_le(const uint8_t *data, const size_t len)
{
    int64_t value = 0;
    for (size_t i = 0; i < len && i < 8; i++) {
        value |= ((int64_t)data[i]) << (8 * i);
    }
    return value;
}

#define UVC_CTRL_ON_SET(field)                                                       \
    static void uvc_ctrl_on_set_##field(const char *name, const uint8_t *data, size_t len) \
    {                                                                                \
        int64_t value = read_value_le(data, len);                                    \
        g_uvc_ctrl_state.field = value;                                              \
        if (s_value_cb) {                                                            \
            s_value_cb(name, value);                                                 \
        }                                                                            \
    }

#define UVC_CTRL_DECLARE_HANDLER(ent_id, selector, entry_name, len, cur_val, min_val, max_val, res_val, def_val, field) \
    UVC_CTRL_ON_SET(field)

UVC_CTRL_PARAM_DECL_LIST(UVC_CTRL_DECLARE_HANDLER)

#undef UVC_CTRL_DECLARE_HANDLER

#define UVC_CTRL_ENTRY(ent_id, selector, entry_name, len, cur_val, min_val, max_val, res_val, def_val, field) \
    {                                                                             \
        .entity_id = (ent_id),                                                    \
        .control_selector = (selector),                                           \
        .name = (entry_name),                                                     \
        .data_len = (len),                                                        \
        .cur = (cur_val),                                                         \
        .min = (min_val),                                                         \
        .max = (max_val),                                                         \
        .res = (res_val),                                                         \
        .def = (def_val),                                                         \
        .value_ptr = &g_uvc_ctrl_state.field,                                     \
        .on_set = uvc_ctrl_on_set_##field,                                         \
    }

const uvc_ctrl_entry_t g_uvc_ctrl_entries[] = {
    UVC_CTRL_PARAM_LIST(UVC_CTRL_ENTRY),
};

const size_t g_uvc_ctrl_entry_count = sizeof(g_uvc_ctrl_entries) / sizeof(g_uvc_ctrl_entries[0]);
