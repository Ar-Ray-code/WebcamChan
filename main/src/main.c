#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_camera.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "img_converters.h"
#include "bsp/esp-bsp.h"
#include "lvgl.h"
#include "usb_device_uvc.h"
#include "camera_pins.h"
#include "uvc_ctrl_registry.h"
#include "uvc_ctrl_params.h"
#include "uvc_ctrl_state.h"
#include "avatar.h"

// UVC Stream Configuration (must match usb_device_uvc component config)
#define UVC_FRAME_WIDTH     320
#define UVC_FRAME_HEIGHT    240
#define UVC_FRAME_RATE      30  // Device reports 30fps for 320x240

// UVC Buffer size (must be larger than single frame)
#define UVC_BUFFER_SIZE     (40 * 1024)

// RGB565 byte order adjustment before JPEG conversion (usually OFF).
#define UVC_RGB565_BYTE_SWAP 0

// Display dimensions
#define DISPLAY_WIDTH   320
#define DISPLAY_HEIGHT  240

// LVGL UI objects
static lv_obj_t *status_label = NULL;
static lv_obj_t *fps_label = NULL;
static lv_obj_t *info_label = NULL;
static lv_obj_t *log_label = NULL;
static lv_obj_t *camera_dot = NULL;

// Log buffer for LCD display
#define LOG_LINES 6
#define LOG_LINE_LEN 40
static char log_buffer[LOG_LINES][LOG_LINE_LEN];

static volatile int g_brightness = 128;
static int current_brightness = -1;

static void uvc_ctrl_value_log(const char *name, int64_t value)
{
    if (strcmp(name, "Brightness") == 0) {
        if (value < 0) {
            value = 0;
        } else if (value > 255) {
            value = 255;
        }
        g_brightness = (int)value;
    }
}

// UVC streaming state
static volatile bool uvc_streaming = false;
static volatile uint32_t frame_count = 0;
static int64_t last_fps_time = 0;
static float current_fps = 0.0f;
static uint8_t *uvc_buffer = NULL;
static uint8_t *jpeg_buffer = NULL;
static size_t jpeg_buffer_size = 0;
static uvc_fb_t uvc_frame;

// Keep track of current camera frame for returning
static camera_fb_t *current_camera_fb = NULL;
static bool first_frame_logged = false;

static esp_err_t init_camera(void)
{
    camera_config_t config = BSP_CAMERA_DEFAULT_CONFIG;

    config.pixel_format = PIXFORMAT_RGB565;
    config.frame_size = FRAMESIZE_QVGA;
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        return err;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (s != NULL) {
        s->set_vflip(s, BSP_CAMERA_VFLIP);
        s->set_hmirror(s, BSP_CAMERA_HMIRROR);
    }
    return ESP_OK;
}


static inline void rgb565_swap_bytes(uint16_t *buf, size_t pixel_count)
{
    for (size_t i = 0; i < pixel_count; i++) {
        uint16_t v = buf[i];
        buf[i] = (uint16_t)((v >> 8) | (v << 8));
    }
}

static esp_err_t uvc_input_start_cb(uvc_format_t format, int width, int height, int rate, void *cb_ctx)
{
    uvc_streaming = true;
    first_frame_logged = false;
    frame_count = 0;
    last_fps_time = esp_timer_get_time();
    return ESP_OK;
}

// Callback to get frame buffer from camera
// Captures RGB565 frame and converts to JPEG for USB streaming
static uvc_fb_t *uvc_input_fb_get_cb(void *cb_ctx)
{
    if (!uvc_streaming) {
        return NULL;
    }

    camera_fb_t *fb = NULL;
    for (int retry = 0; retry < 3; retry++) {
        fb = esp_camera_fb_get();
        if (fb != NULL) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (fb == NULL) {
        return NULL;
    }

    current_camera_fb = fb;

    // Convert RGB565 to JPEG (frame2jpg allocates buffer internally)
    uint8_t *out_buf = NULL;
    size_t out_len = 0;
    bool converted = frame2jpg(fb, 80, &out_buf, &out_len);

    if (!converted || out_buf == NULL || out_len == 0) {
        esp_camera_fb_return(fb);
        current_camera_fb = NULL;
        return NULL;
    }

    // Store JPEG buffer pointer for cleanup later
    jpeg_buffer = out_buf;
    jpeg_buffer_size = out_len;

    // Log first frame info for each session
    if (!first_frame_logged) {
        first_frame_logged = true;
    }

    // Fill UVC frame structure with JPEG data
    uvc_frame.buf = jpeg_buffer;
    uvc_frame.len = jpeg_buffer_size;
    uvc_frame.width = fb->width;
    uvc_frame.height = fb->height;
    uvc_frame.format = UVC_FORMAT_JPEG;
    uvc_frame.timestamp = fb->timestamp;

    return &uvc_frame;
}

// Callback to return frame buffer to camera
static void uvc_input_fb_return_cb(uvc_fb_t *fb, void *cb_ctx)
{
    if (jpeg_buffer != NULL) {
        free(jpeg_buffer);
        jpeg_buffer = NULL;
        jpeg_buffer_size = 0;
    }

    if (current_camera_fb != NULL) {
        esp_camera_fb_return(current_camera_fb);
        current_camera_fb = NULL;
    }

    frame_count++;
}

// Called when host stops streaming
static void uvc_input_stop_cb(void *cb_ctx)
{
    uvc_streaming = false;

    if (jpeg_buffer != NULL) {
        free(jpeg_buffer);
        jpeg_buffer = NULL;
        jpeg_buffer_size = 0;
    }

    if (current_camera_fb != NULL) {
        esp_camera_fb_return(current_camera_fb);
        current_camera_fb = NULL;
    }
}

static esp_err_t init_usb_uvc(void)
{
    uvc_buffer = heap_caps_malloc(UVC_BUFFER_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (uvc_buffer == NULL) {
        uvc_buffer = heap_caps_malloc(UVC_BUFFER_SIZE, MALLOC_CAP_DEFAULT);
        if (uvc_buffer == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    // UVC device configuration
    uvc_device_config_t uvc_config = {
        .uvc_buffer = uvc_buffer,
        .uvc_buffer_size = UVC_BUFFER_SIZE,
        .start_cb = uvc_input_start_cb,
        .fb_get_cb = uvc_input_fb_get_cb,
        .fb_return_cb = uvc_input_fb_return_cb,
        .stop_cb = uvc_input_stop_cb,
        .cb_ctx = NULL,
    };

    esp_err_t err = uvc_device_config(0, &uvc_config);
    if (err != ESP_OK) {
        return err;
    }

    err = uvc_device_init();
    if (err != ESP_OK) {
        return err;
    }

    return ESP_OK;
}

static void create_ui(void)
{
    memset(log_buffer, 0, sizeof(log_buffer));

    bsp_display_lock(0);
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), 0);

    status_label = NULL;
    fps_label = NULL;
    info_label = NULL;

    stackchanface_init(lv_scr_act());
    log_label = NULL;

    camera_dot = lv_obj_create(lv_scr_act());
    lv_obj_set_size(camera_dot, 8, 8);
    lv_obj_set_style_radius(camera_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(camera_dot, lv_color_hex(0xff0000), 0);
    lv_obj_set_style_bg_opa(camera_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(camera_dot, 0, 0);
    lv_obj_set_style_outline_width(camera_dot, 0, 0);
    lv_obj_set_style_shadow_width(camera_dot, 0, 0);
    lv_obj_align(camera_dot, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_add_flag(camera_dot, LV_OBJ_FLAG_HIDDEN);

    bsp_display_unlock();
}

/**
 * UI update task
 */
static void ui_task(void *arg)
{
    static bool blink_on = false;
    static int64_t last_blink_time = 0;

    for (;;) {
        // Update FPS calculation (for logging purposes only, no display)
        if (uvc_streaming) {
            int64_t now = esp_timer_get_time();
            int64_t elapsed = now - last_fps_time;

            if (elapsed >= 1000000) {  // 1 second
                current_fps = (float)frame_count * 1000000.0f / elapsed;
                frame_count = 0;
                last_fps_time = now;
            }
        }

        // Blink red dot while camera is streaming (1Hz)
        if (uvc_streaming) {
            int64_t now = esp_timer_get_time();
            if (now - last_blink_time >= 500000) {  // 0.5s toggle => 1Hz blink
                last_blink_time = now;
                blink_on = !blink_on;
                bsp_display_lock(0);
                if (blink_on) {
                    lv_obj_clear_flag(camera_dot, LV_OBJ_FLAG_HIDDEN);
                } else {
                    lv_obj_add_flag(camera_dot, LV_OBJ_FLAG_HIDDEN);
                }
                bsp_display_unlock();
            }
        } else {
            if (blink_on) {
                blink_on = false;
                bsp_display_lock(0);
                lv_obj_add_flag(camera_dot, LV_OBJ_FLAG_HIDDEN);
                bsp_display_unlock();
            }
            last_blink_time = 0;
        }

        // Update avatar eye shape based on UVC gain control
        int pending = g_brightness;
        if (pending >= 0 && pending <= 255 && pending != current_brightness) {
            current_brightness = pending;
            bsp_display_lock(0);
            stackchanface_set_brightness((uint8_t)pending);
            bsp_display_unlock();
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    bsp_display_start();
    bsp_display_backlight_on();
    bsp_display_brightness_set(80);

    create_ui();

    // Register UVC control parameters
    uvc_ctrl_registry_register(g_uvc_ctrl_entries, g_uvc_ctrl_entry_count);
    uvc_ctrl_state_set_callback(uvc_ctrl_value_log);

    // Initialize camera
    esp_err_t err = init_camera();
    if (err != ESP_OK) {
        bsp_display_lock(0);
        lv_obj_set_style_text_color(status_label, lv_color_hex(0xff0000), 0);
        bsp_display_unlock();

        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    // Wait for camera to start capturing
    vTaskDelay(pdMS_TO_TICKS(500));

    // Test camera capture before starting UVC (with retries)
    camera_fb_t *test_fb = NULL;
    for (int i = 0; i < 10; i++) {
        test_fb = esp_camera_fb_get();
        if (test_fb) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    if (test_fb) {
        esp_camera_fb_return(test_fb);
    }

    // Small delay to ensure camera is stable
    vTaskDelay(pdMS_TO_TICKS(100));

    err = init_usb_uvc();
    if (err != ESP_OK) {
        bsp_display_lock(0);
        lv_obj_set_style_text_color(status_label, lv_color_hex(0xff0000), 0);
        bsp_display_unlock();

        while (1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    // Start UI update task
    xTaskCreatePinnedToCore(ui_task, "ui_task", 4096, NULL, 3, NULL, 1);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
