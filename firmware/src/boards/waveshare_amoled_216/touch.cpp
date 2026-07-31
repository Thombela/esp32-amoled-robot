#include "../../hal/touch_hal.h"
#include "../../hal/imu_hal.h"
#include "board.h"
#include <Arduino.h>
#include <Wire.h>
#include <TouchDrvCSTXXX.hpp>

static TouchDrvCST92xx touch;

static volatile bool     touch_data_ready = false;
static volatile bool     touch_pressed = false;
static volatile uint16_t touch_x = 0;
static volatile uint16_t touch_y = 0;

static void IRAM_ATTR touch_isr(void) {
    touch_data_ready = true;
}

void touch_hal_init(void) {
    touch.setPins(TP_RST, TP_INT);
    if (!touch.begin(Wire, CST9220_ADDR, IIC_SDA, IIC_SCL)) {
        Serial.println("Touch init failed");
        return;
    }
    touch.setMaxCoordinates(LCD_WIDTH, LCD_HEIGHT);
    touch.setSwapXY(true);
    touch.setMirrorXY(true, false);
    pinMode(TP_INT, INPUT_PULLUP);
    attachInterrupt(TP_INT, touch_isr, FALLING);
    Serial.println("Touch init OK");
}

// Per-rotation touch correction, derived directly from hardware: logged raw
// controller output (before any correction) against a deliberate 4-corner
// tap + two cardinal swipes at each of the 4 physical orientations, with
// imu_hal_rotation_quadrant() read back at the same time to know which
// rotation index the IMU actually assigns to which orientation (it does NOT
// match the naive "0 = the orientation with buttons on top" assumption —
// that orientation is rotation 3). The 4 resulting formulas below form a
// proper rotation group (rot0 and rot2 are exact inverses of each other;
// applying either twice gives rot1's 180° formula), which is strong
// independent confirmation on top of the hardware match:
//   rotation 3 — buttons on top ("original"): raw already matches 1:1, no
//                correction needed.
//   rotation 1 — buttons on bottom ("upside down"): 180°, both axes flip.
//   rotation 2 — buttons on right: touch chip's raw frame is 90° CCW of the
//                display's, so the correction rotates 90° CW.
//   rotation 0 — buttons on left: the other direction, 90° CCW.
// Two earlier attempts at this (a swap-based "true rotation" guess, then an
// unconditional flip-based patch) were each fixed on wrong or incomplete
// data — don't reintroduce either without a fresh 4-corner+swipe log per
// rotation like the one that produced this table.
static void correct_touch(volatile uint16_t* x, volatile uint16_t* y, uint8_t rot) {
    const uint16_t S = LCD_WIDTH;  // square panel — LCD_WIDTH == LCD_HEIGHT
    uint16_t rx = *x, ry = *y;
    switch (rot) {
    case 0: *x = ry;         *y = S - 1 - rx; break;
    case 1: *x = S - 1 - rx; *y = S - 1 - ry; break;
    case 2: *x = S - 1 - ry; *y = rx;         break;
    case 3: default:                          break;  // identity
    }
}

void touch_hal_read(uint16_t* x, uint16_t* y, bool* pressed) {
    if (touch_data_ready) {
        touch_data_ready = false;
        int16_t tx[5], ty[5];
        uint8_t n = touch.getPoint(tx, ty, touch.getSupportTouchPoint());
        if (n > 0) {
            touch_pressed = true;
            touch_x = (uint16_t)tx[0];
            touch_y = (uint16_t)ty[0];
            correct_touch(&touch_x, &touch_y, imu_hal_rotation_quadrant());
        } else {
            touch_pressed = false;
        }
    }
    *x = touch_x;
    *y = touch_y;
    *pressed = touch_pressed;
}
