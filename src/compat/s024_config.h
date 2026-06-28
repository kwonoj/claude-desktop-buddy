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

// ── Panel geometry (native, portrait) ──────────────────────────────────
#define S024_TFT_W 240
#define S024_TFT_H 320

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
#define S024_TOUCH_SWAP_XY   1
#define S024_TOUCH_INVERT_X  0
#define S024_TOUCH_INVERT_Y  1
// Pressure gate: ignore feather-light/ghost touches below this Z.
#define S024_TOUCH_Z_MIN     250

#define S024_TOUCH_DEBUG 0

// ── Display inversion ──────────────────────────────────────────────────
// Authoritative configs say OFF for this board. If colors come out
// inverted (white<->black), set to 1.
#define S024_INVERT_DISPLAY 0

// ── Boot self-test (R/G/B flash) ───────────────────────────────────────
#define S024_BOOT_SELFTEST 0

// ── On-screen A/B button bar ───────────────────────────────────────────
// The buddy sprite is 135x240; it's pushed into the top of the 240x320
// panel and the freed strip at the bottom becomes the two touch targets.
#define S024_BAR_H 64
#define S024_BAR_Y (S024_TFT_H - S024_BAR_H)   // 256

// ── Buddy sprite placement ─────────────────────────────────────────────
// The app sprite is 135 wide x 240 tall. Scale it up to fill the 240-wide
// panel above the button bar. 240/135 ≈ 1.78; cap the height so it leaves
// room for the bar (256px usable).
#define S024_SPR_W 135
#define S024_SPR_H 240
#define S024_SPR_ZOOM_X 1.777f          // 135 -> ~240
#define S024_SPR_ZOOM_Y 1.066f          // 240 -> ~256 (fills to the bar)
#define S024_SPR_CX (S024_TFT_W / 2)    // horizontal center
#define S024_SPR_CY (S024_BAR_Y / 2)    // vertical center of the usable area

// ── Input timing ───────────────────────────────────────────────────────
#define S024_LONGPRESS_MS 600
