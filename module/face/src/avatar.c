#include <stdbool.h>
#include "avatar.h"

typedef struct {
    lv_obj_t *container;
    lv_obj_t *eye_l;
    lv_obj_t *eye_r;
    lv_obj_t *eye_mask_l;
    lv_obj_t *eye_mask_r;
    lv_obj_t *mouth_line;
    lv_point_precise_t mouth_pts[2];
    uint8_t current_brightness;
    bool inited;
} stackchanface_avatar_t;

static stackchanface_avatar_t s_avatar = {0};

static void set_mouth_line(int16_t x1, int16_t y1, int16_t x2, int16_t y2)
{
    s_avatar.mouth_pts[0].x = x1;
    s_avatar.mouth_pts[0].y = y1;
    s_avatar.mouth_pts[1].x = x2;
    s_avatar.mouth_pts[1].y = y2;
    lv_line_set_points(s_avatar.mouth_line, s_avatar.mouth_pts, 2);
}

static void show_mouth_line(bool show)
{
    if (show) {
        lv_obj_clear_flag(s_avatar.mouth_line, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_avatar.mouth_line, LV_OBJ_FLAG_HIDDEN);
    }
}

static void set_eye_height(int16_t h)
{
    lv_obj_set_height(s_avatar.eye_l, h);
    lv_obj_set_height(s_avatar.eye_r, h);
    lv_obj_set_style_radius(s_avatar.eye_l, h / 2, 0);
    lv_obj_set_style_radius(s_avatar.eye_r, h / 2, 0);
}

static void set_eye_offset(int16_t dx_l, int16_t dx_r, int16_t dy)
{
    // BASIC EYE position: (-60, -20) and (60, -20)
    lv_obj_align(s_avatar.eye_l, LV_ALIGN_CENTER, -60 + dx_l, -20 + dy);
    lv_obj_align(s_avatar.eye_r, LV_ALIGN_CENTER, 60 + dx_r, -20 + dy);
}

static void set_eye_mask_offset(int16_t offset)
{
    if (offset <= 0) {
        lv_obj_add_flag(s_avatar.eye_mask_l, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_avatar.eye_mask_r, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_translate_y(s_avatar.eye_mask_l, 0, 0);
        lv_obj_set_style_translate_y(s_avatar.eye_mask_r, 0, 0);
        return;
    }

    lv_obj_clear_flag(s_avatar.eye_mask_l, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_avatar.eye_mask_r, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align_to(s_avatar.eye_mask_l, s_avatar.eye_l, LV_ALIGN_CENTER, 0, 0);
    lv_obj_align_to(s_avatar.eye_mask_r, s_avatar.eye_r, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_translate_y(s_avatar.eye_mask_l, offset, 0);
    lv_obj_set_style_translate_y(s_avatar.eye_mask_r, offset, 0);
}

static uint8_t clamp_brightness(int64_t brightness)
{
    if (brightness < 0) {
        return 0;
    }
    if (brightness > 255) {
        return 255;
    }
    return (uint8_t)brightness;
}

void stackchanface_init(lv_obj_t *parent)
{
    if (s_avatar.inited) {
        return;
    }
    s_avatar.inited = true;

    if (!parent) {
        parent = lv_scr_act();
    }

    s_avatar.container = lv_obj_create(parent);
    lv_obj_remove_style_all(s_avatar.container);
    lv_obj_set_size(s_avatar.container, 320, 240);
    lv_obj_center(s_avatar.container);

    lv_color_t bg_color = lv_obj_get_style_bg_color(parent, 0);

    // ============ BASIC EYE ================
    s_avatar.eye_l = lv_obj_create(s_avatar.container);
    lv_obj_set_size(s_avatar.eye_l, 18, 18);
    lv_obj_set_style_radius(s_avatar.eye_l, 9, 0);
    lv_obj_set_style_bg_color(s_avatar.eye_l, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_border_width(s_avatar.eye_l, 0, 0);
    lv_obj_align(s_avatar.eye_l, LV_ALIGN_CENTER, -60, -20);

    s_avatar.eye_r = lv_obj_create(s_avatar.container);
    lv_obj_set_size(s_avatar.eye_r, 18, 18);
    lv_obj_set_style_radius(s_avatar.eye_r, 9, 0);
    lv_obj_set_style_bg_color(s_avatar.eye_r, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_border_width(s_avatar.eye_r, 0, 0);
    lv_obj_align(s_avatar.eye_r, LV_ALIGN_CENTER, 60, -20);

    s_avatar.eye_mask_l = lv_obj_create(s_avatar.container);
    lv_obj_set_size(s_avatar.eye_mask_l, 18, 18);
    lv_obj_set_style_radius(s_avatar.eye_mask_l, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_avatar.eye_mask_l, bg_color, 0);
    lv_obj_set_style_bg_opa(s_avatar.eye_mask_l, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_avatar.eye_mask_l, 0, 0);
    lv_obj_set_style_outline_width(s_avatar.eye_mask_l, 0, 0);
    lv_obj_set_style_shadow_width(s_avatar.eye_mask_l, 0, 0);
    lv_obj_set_style_pad_all(s_avatar.eye_mask_l, 0, 0);
    lv_obj_align(s_avatar.eye_mask_l, LV_ALIGN_CENTER, 0, 0);

    s_avatar.eye_mask_r = lv_obj_create(s_avatar.container);
    lv_obj_set_size(s_avatar.eye_mask_r, 18, 18);
    lv_obj_set_style_radius(s_avatar.eye_mask_r, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_avatar.eye_mask_r, bg_color, 0);
    lv_obj_set_style_bg_opa(s_avatar.eye_mask_r, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_avatar.eye_mask_r, 0, 0);
    lv_obj_set_style_outline_width(s_avatar.eye_mask_r, 0, 0);
    lv_obj_set_style_shadow_width(s_avatar.eye_mask_r, 0, 0);
    lv_obj_set_style_pad_all(s_avatar.eye_mask_r, 0, 0);
    lv_obj_align(s_avatar.eye_mask_r, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(s_avatar.eye_mask_l, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_avatar.eye_mask_r, LV_OBJ_FLAG_HIDDEN);

    // ============= BASIC MOUTH =============
    s_avatar.mouth_pts[0].x = -53;
    s_avatar.mouth_pts[0].y = 0;
    s_avatar.mouth_pts[1].x = 53;
    s_avatar.mouth_pts[1].y = 0;
    s_avatar.mouth_line = lv_line_create(s_avatar.container);
    lv_line_set_points(s_avatar.mouth_line, s_avatar.mouth_pts, 2);
    lv_obj_set_style_line_width(s_avatar.mouth_line, 5, 0);
    lv_obj_set_style_line_color(s_avatar.mouth_line, lv_color_hex(0xffffff), 0);
    lv_obj_align(s_avatar.mouth_line, LV_ALIGN_CENTER, 0, 30);

    s_avatar.current_brightness = 255;
    stackchanface_set_brightness(128);
}

void stackchanface_set_brightness(uint8_t brightness)
{
    if (!s_avatar.inited) {
        return;
    }
    if (brightness == s_avatar.current_brightness) {
        return;
    }

    s_avatar.current_brightness = brightness;

    // Reset to BASIC positions
    show_mouth_line(true);
    set_mouth_line(-53, 0, 53, 0);  // BASIC MOUTH width
    set_eye_offset(0, 0, 0);  // BASIC EYE position

    // 0 -> sad, 128 -> neutral, 255 -> happy
    uint8_t clamped = clamp_brightness(brightness);
    const uint8_t smile_start = 170;
    const int16_t mask_base_offset = 12;  // below eye center
    const int16_t mask_min_offset = 2;    // still below eye center
    int16_t eye_height = 18;
    int16_t eye_dy = 0;
    int16_t mask_offset = 0;
    if (clamped <= 128) {
        eye_height = 6 + (int16_t)((12 * clamped) / 128);
        eye_dy = (int16_t)((6 * (128 - clamped)) / 128);
        mask_offset = 0;
    } else {
        eye_height = 18;
        eye_dy = 0;
        if (clamped >= smile_start) {
            int16_t range = mask_base_offset - mask_min_offset;
            mask_offset = mask_base_offset - (int16_t)((range * (clamped - smile_start)) / (255 - smile_start));
        } else {
            mask_offset = 0;
        }
    }
    set_eye_height(eye_height);
    set_eye_offset(0, 0, eye_dy);
    set_eye_mask_offset(mask_offset);
}
