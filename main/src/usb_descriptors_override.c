/**
 * Override the USB configuration descriptor to include a Processing Unit
 * in the UVC Video Control interface topology, and intercept
 * videod_control_xfer_cb to handle entity control requests that TinyUSB's
 * built-in video driver does not support.
 *
 * Original topology:  Camera Terminal (0x01) -> Output Terminal (0x02)
 * New topology:       Camera Terminal (0x01) -> Processing Unit (0x02) -> Output Terminal (0x03)
 *
 * Uses the linker --wrap option to intercept tud_descriptor_configuration_cb
 * and videod_control_xfer_cb without modifying managed_components.
 */

#include <string.h>
#include "tusb.h"
#include "class/video/video.h"
#include "usb_descriptors.h"
#include "uvc_ctrl_registry.h"

/* ======================================================================
 * Part 1: Configuration Descriptor with Processing Unit
 * ====================================================================== */

/* Entity IDs for our custom descriptor topology */
#define MY_ENTITY_CAMERA_TERMINAL  0x01
#define MY_ENTITY_PROCESSING_UNIT  0x02
#define MY_ENTITY_OUTPUT_TERMINAL  0x03

/* Processing Unit descriptor (UVC 1.5, bControlSize=3)
 *
 * Fields:
 *   bLength(1) + bDescriptorType(1) + bDescriptorSubtype(1) + bUnitID(1) +
 *   bSourceID(1) + wMaxMultiplier(2) + bControlSize(1) + bmControls(3) +
 *   iProcessing(1) + bmVideoStandards(1) = 13
 */
#define PU_DESC_LEN  13

/*
 * bmControls bitmap for Processing Unit:
 *   Byte 0 bits: [0]Brightness [1]Contrast [2]Hue [3]Saturation
 *   Byte 1 bits: [3]HueAuto [4]WhiteBalanceTempAuto
 *   Byte 2: unused
 */
#define PU_BM_CTRL_0  0x0F
#define PU_BM_CTRL_1  0x18
#define PU_BM_CTRL_2  0x00

#define TUD_VIDEO_DESC_PROCESSING_UNIT(_unitID, _srcID, _bm0, _bm1, _bm2) \
    PU_DESC_LEN, TUSB_DESC_CS_INTERFACE, VIDEO_CS_ITF_VC_PROCESSING_UNIT, \
    _unitID, _srcID, U16_TO_U8S_LE(0x0000), \
    0x03, _bm0, _bm1, _bm2, \
    0x00, 0x00

/* Total configuration descriptor length = original + Processing Unit */
#define MY_CONFIG_TOTAL_LEN \
    (TUD_CONFIG_DESC_LEN + TUD_VIDEO_CAPTURE_DESC_MULTI_MJPEG_LEN(4) + PU_DESC_LEN)

#define MY_EPNUM_VIDEO_IN  0x81

static uint8_t const my_desc_fs_configuration[] = {
    /* Configuration Descriptor */
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, MY_CONFIG_TOTAL_LEN, 0, 500),

    /* ---- Video Control Interface ---- */
    TUD_VIDEO_DESC_IAD(ITF_NUM_VIDEO_CONTROL, 0x02, 4),
    TUD_VIDEO_DESC_STD_VC(ITF_NUM_VIDEO_CONTROL, 0, 4),
    TUD_VIDEO_DESC_CS_VC(
        0x0150,
        TUD_VIDEO_DESC_CAMERA_TERM_LEN + PU_DESC_LEN + TUD_VIDEO_DESC_OUTPUT_TERM_LEN,
        UVC_CLOCK_FREQUENCY,
        ITF_NUM_VIDEO_STREAMING),
    /* Camera Terminal (entity 1) */
    TUD_VIDEO_DESC_CAMERA_TERM(MY_ENTITY_CAMERA_TERMINAL, 0, 0, 0, 0, 0, 0),
    /* Processing Unit (entity 2), source = Camera Terminal */
    TUD_VIDEO_DESC_PROCESSING_UNIT(
        MY_ENTITY_PROCESSING_UNIT, MY_ENTITY_CAMERA_TERMINAL,
        PU_BM_CTRL_0, PU_BM_CTRL_1, PU_BM_CTRL_2),
    /* Output Terminal (entity 3), source = Processing Unit */
    TUD_VIDEO_DESC_OUTPUT_TERM(
        MY_ENTITY_OUTPUT_TERMINAL, VIDEO_TT_STREAMING, 0,
        MY_ENTITY_PROCESSING_UNIT, 0),

    /* ---- Video Streaming Interface (alt 0) ---- */
    TUD_VIDEO_DESC_STD_VS(ITF_NUM_VIDEO_STREAMING, 0, 0, 4),
    TUD_VIDEO_DESC_CS_VS_INPUT(
        1,
        TUD_VIDEO_DESC_CS_VS_FMT_MJPEG_LEN
            + (UVC_FRAME_NUM * TUD_VIDEO_DESC_CS_VS_FRM_MJPEG_CONT_LEN)
            + TUD_VIDEO_DESC_CS_VS_COLOR_MATCHING_LEN,
        MY_EPNUM_VIDEO_IN, 0,
        MY_ENTITY_OUTPUT_TERMINAL,
        0, 0, 0,
        0),
    TUD_VIDEO_DESC_CS_VS_FMT_MJPEG(
        1, UVC_FRAME_NUM, 0, 1, 0, 0, 0, 0),
    TUD_VIDEO_DESC_CS_VS_FRM_MJPEG_CONT_TEMPLATE(0, 1),
    TUD_VIDEO_DESC_CS_VS_FRM_MJPEG_CONT_TEMPLATE(0, 2),
    TUD_VIDEO_DESC_CS_VS_FRM_MJPEG_CONT_TEMPLATE(0, 3),
    TUD_VIDEO_DESC_CS_VS_FRM_MJPEG_CONT_TEMPLATE(0, 4),
    TUD_VIDEO_DESC_CS_VS_COLOR_MATCHING(
        VIDEO_COLOR_PRIMARIES_BT709,
        VIDEO_COLOR_XFER_CH_BT709,
        VIDEO_COLOR_COEF_SMPTE170M),

    /* ---- Video Streaming Interface (alt 1) + ISO Endpoint ---- */
    TUD_VIDEO_DESC_STD_VS(ITF_NUM_VIDEO_STREAMING, 1, 1, 4),
    TUD_VIDEO_DESC_EP_ISO(MY_EPNUM_VIDEO_IN, CFG_TUD_CAM1_VIDEO_STREAMING_EP_BUFSIZE, 1),
};

_Static_assert(sizeof(my_desc_fs_configuration) == MY_CONFIG_TOTAL_LEN,
               "Configuration descriptor size mismatch");

uint8_t const *__wrap_tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return my_desc_fs_configuration;
}

/* ======================================================================
 * Part 2: Entity Control Request Handler
 *
 * TinyUSB's video driver does not handle entity control requests
 * (entity_id != 0). It only verifies the entity exists in the descriptor
 * and returns success without completing the USB control transfer,
 * causing a timeout. We intercept videod_control_xfer_cb to handle
 * Processing Unit control requests properly.
 * ====================================================================== */

extern bool __real_videod_control_xfer_cb(uint8_t rhport, uint8_t stage,
                                          tusb_control_request_t const *request);

static uint8_t s_entity_ctrl_buf[4];

static bool handle_entity_control_request(uint8_t rhport, uint8_t stage,
                                          tusb_control_request_t const *request)
{
    uint8_t entity_id = TU_U16_HIGH(request->wIndex);
    uint8_t control_selector = TU_U16_HIGH(request->wValue);
    uint8_t bRequest = request->bRequest;
    uint16_t wLength = request->wLength;
    uint16_t len = (wLength < sizeof(s_entity_ctrl_buf))
                    ? wLength : sizeof(s_entity_ctrl_buf);

    if (stage == CONTROL_STAGE_SETUP) {
        switch (bRequest) {
        case VIDEO_REQUEST_GET_INFO:
        case VIDEO_REQUEST_GET_CUR:
        case VIDEO_REQUEST_GET_MIN:
        case VIDEO_REQUEST_GET_MAX:
        case VIDEO_REQUEST_GET_RES:
        case VIDEO_REQUEST_GET_DEF:
        case VIDEO_REQUEST_GET_LEN: {
            memset(s_entity_ctrl_buf, 0, sizeof(s_entity_ctrl_buf));
            int err = uvc_ctrl_registry_handle(entity_id, control_selector,
                                               bRequest, stage,
                                               s_entity_ctrl_buf, len);
            if (err != VIDEO_ERROR_NONE) {
                return false;
            }
            return tud_control_xfer(rhport, request, s_entity_ctrl_buf, len);
        }
        case VIDEO_REQUEST_SET_CUR:
            memset(s_entity_ctrl_buf, 0, sizeof(s_entity_ctrl_buf));
            return tud_control_xfer(rhport, request, s_entity_ctrl_buf, len);
        default:
            return false;
        }
    } else if (stage == CONTROL_STAGE_DATA) {
        if (bRequest == VIDEO_REQUEST_SET_CUR) {
            int err = uvc_ctrl_registry_handle(entity_id, control_selector,
                                               bRequest, stage,
                                               s_entity_ctrl_buf, len);
            if (err != VIDEO_ERROR_NONE) {
                return false;
            }
        }
    }
    return true;
}

bool __wrap_videod_control_xfer_cb(uint8_t rhport, uint8_t stage,
                                   tusb_control_request_t const *request)
{
    if (request->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS &&
        request->bmRequestType_bit.recipient == TUSB_REQ_RCPT_INTERFACE) {
        uint8_t entity_id = TU_U16_HIGH(request->wIndex);
        if (entity_id != 0) {
            return handle_entity_control_request(rhport, stage, request);
        }
    }
    return __real_videod_control_xfer_cb(rhport, stage, request);
}
