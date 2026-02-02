#ifndef CAMERA_PINS_H
#define CAMERA_PINS_H

// Power/Reset (managed externally via LTR-553)
#define CAM_PIN_PWDN    -1
#define CAM_PIN_RESET   -1

// Clock
#define CAM_PIN_XCLK    2

// I2C (SCCB)
#define CAM_PIN_SIOD    12
#define CAM_PIN_SIOC    11

// Data pins (D0-D7)
#define CAM_PIN_D0      15
#define CAM_PIN_D1      17
#define CAM_PIN_D2      18
#define CAM_PIN_D3      8
#define CAM_PIN_D4      10
#define CAM_PIN_D5      16
#define CAM_PIN_D6      14
#define CAM_PIN_D7      9

// Sync signals
#define CAM_PIN_VSYNC   6
#define CAM_PIN_HREF    7
#define CAM_PIN_PCLK    13

#define CAM_XCLK_FREQ_HZ    20000000

#endif // CAMERA_PINS_H
