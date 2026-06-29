#pragma once

// ESP32-2432S024R (Sunton "Cheap Yellow Display", 2.4") board config.
// All board-specific tunables live here. The S024R has:
//   - ST7789 240x320 SPI panel on the HSPI bus
//   - XPT2046 resistive touch sharing that same HSPI bus (CS 33, IRQ 36)
//   - active-high backlight on GPIO 27
//   - onboard common-anode RGB LED (active-LOW) on 4/16/17
//   - no IMU, no buzzer, no RTC chip, no battery gauge, no PSRAM
//
// Pin sources: rzeldent/platformio-espressif32-sunton esp32-2432S024R.json,
// Arduino forum "Another CYD issue (ESP32-2432S024C) [Solved]", and the
// elik745i/ESP32-2432s024c platformio.ini. See .kilo/plans/esp32-2432s024-port.md.

// ── Orientation ─────────────────────────────────────────────────────────
// Portrait, with the A/B bar along the bottom. The app's sprite is portrait
// (135x240) so this matches its native layout.
//   0/2 = portrait (240x320), 1/3 = landscape (320x240).
// If the image is upside-down for how you hold the board, change 0 -> 2.
#define S024_ROTATION 0

#if (S024_ROTATION & 1)
  #define S024_TFT_W 320
  #define S024_TFT_H 240
#else
  #define S024_TFT_W 240
  #define S024_TFT_H 320
#endif

// ── Backlight ──────────────────────────────────────────────────────────
#define S024_BL_PIN  27
#define S024_BL_FULL 255     // analogWrite full-scale (8-bit LEDC)

// ── Onboard RGB LED (common-anode → active LOW) ────────────────────────
// Used for the "attention" blink in place of the M5StickC's GPIO10 LED.
#define S024_LED_R 4
#define S024_LED_G 16
#define S024_LED_B 17

// ── Touch (XPT2046, resistive) ─────────────────────────────────────────
// Shares the display HSPI bus (SCLK 14 / MOSI 13 / MISO 12); only CS+IRQ
// differ. We drive it with a dedicated XPT2046_Touchscreen instance on a
// separate SPIClass(HSPI) handle in the shim.
#define S024_TOUCH_CS  33
#define S024_TOUCH_IRQ 36
#define S024_TOUCH_SCLK 14
#define S024_TOUCH_MOSI 13
#define S024_TOUCH_MISO 12

// Raw XPT2046 reads are 12-bit (0..4095). These map raw → screen pixels.
// Conservative defaults for a portrait S024R; tune on hardware if taps land
// off-target. Swap/invert flip the axes for different panel batches.
#define S024_TOUCH_RAW_MIN_X 300
#define S024_TOUCH_RAW_MAX_X 3900
#define S024_TOUCH_RAW_MIN_Y 300
#define S024_TOUCH_RAW_MAX_Y 3900
// Orientation: with SWAP_XY on, the panel's touch X axis maps to screen Y.
// On the S024R the X axis reads mirrored vs the display and top/bottom is
// flipped, so invert X and leave Y un-inverted.
#define S024_TOUCH_SWAP_XY   1
#define S024_TOUCH_INVERT_X  1
#define S024_TOUCH_INVERT_Y  0
// Pressure gate: ignore feather-light/ghost touches below this Z.
#define S024_TOUCH_Z_MIN     250

// Set to 1 to log raw+mapped touch coords to serial and draw a crosshair at
// the mapped point — flash once with this on to dial in the orientation and
// raw min/max calibration, then set back to 0.
#define S024_TOUCH_DEBUG 0

// ── Display inversion ──────────────────────────────────────────────────
// Authoritative configs say OFF for this board. If colors come out
// inverted (white<->black), set to 1.
#define S024_INVERT_DISPLAY 0

// ── Boot self-test (R/G/B flash) ───────────────────────────────────────
#define S024_BOOT_SELFTEST 0

// ── Touch input zones (no visible buttons) ──────────────────────────────
// There is NO drawn button bar. The buddy fills the entire screen, and the
// bottom half acts as two invisible touch targets: bottom-left = A,
// bottom-right = B. The top half is "tap to wake / no button".
//   S024_TOUCH_ZONE_Y  : screen Y at/below which a touch counts as A/B.
//   S024_TOUCH_SPLIT_X : screen X dividing A (left) from B (right).
#define S024_TOUCH_ZONE_Y  (S024_TFT_H / 2)   // bottom half
#define S024_TOUCH_SPLIT_X (S024_TFT_W / 2)   // left | right

// The buddy uses the WHOLE screen now.
#define S024_CONTENT_W S024_TFT_W
#define S024_CONTENT_H S024_TFT_H

// ── Buddy sprite placement ─────────────────────────────────────────────
// The app sprite is 135 wide x 240 tall (aspect 0.56). The content region is
// the full panel (S024_CONTENT_W x S024_CONTENT_H). Aspect rarely matches, so
// pick a fit mode:
//
//   0 = STRETCH  fill the whole screen; X and Y scale independently. Mild
//                distortion, but ALL content (info text, HUD, approval
//                prompt) stays on screen. Default.
//   1 = FIT      uniform scale, no distortion; the limiting axis touches the
//                edge and the other letterboxes.
//   2 = WIDTH    uniform scale to fill width; crops a centered vertical band
//                (can hide top/bottom of the sprite). Pet-only.
#define S024_SPR_FIT 0

// Optional manual override of the stretch factors (mode 0 only). 0 = derive
// from the content region. Set both to tune the look, e.g. 1.6f / 1.2f.
#define S024_SPR_ZOOM_X 0.0f
#define S024_SPR_ZOOM_Y 0.0f

// Source crop (in 135x240 sprite pixels) applied BEFORE scaling, to trim
// background-only bands so the meaningful content stretches edge-to-edge.
// The app reserves the top ~70px for the pet and leaves a small bottom gap;
// with an ASCII pet that area is mostly empty. Cropping it and stretching the
// rest fills the panel. Set all to 0 to push the whole sprite unchanged.
//   LEFT/RIGHT trim columns; TOP/BOTTOM trim rows.
#define S024_SPR_CROP_TOP    44
#define S024_SPR_CROP_BOTTOM 18
#define S024_SPR_CROP_LEFT   0
#define S024_SPR_CROP_RIGHT  0

#define S024_SPR_W 135
#define S024_SPR_H 240

// Set to 1 to draw a magenta rectangle around the pushed image region. In
// STRETCH it should hug all four panel edges — if it does, any "margin" you
// see is the app's own background inside the sprite, not the scaler.
#define S024_SPR_DEBUG_BORDER 0

// ── Input timing ───────────────────────────────────────────────────────
#define S024_LONGPRESS_MS 600
