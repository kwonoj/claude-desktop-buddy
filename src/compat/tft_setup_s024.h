#pragma once

// TFT_eSPI "User_Setup" for the ESP32-2432S024R, force-included via
//   build_flags = -include src/compat/tft_setup_s024.h
// so we don't depend on a globally-installed TFT_eSPI User_Setup.h.
//
// USER_SETUP_LOADED tells TFT_eSPI to skip its own User_Setup_Select.h.
// Panel: ST7789 240x320 on the HSPI bus, BGR order, no inversion.
// Touch (XPT2046) is driven separately in the shim, NOT via TFT_eSPI's
// built-in TOUCH_CS path, so we don't define TOUCH_CS here.

#define USER_SETUP_LOADED 1

#define ST7789_DRIVER
// If a panel batch shows a garbled/shifted image, swap the line above for
// ILI9341_DRIVER (pins are identical, only the init sequence differs).

#define TFT_WIDTH  240
#define TFT_HEIGHT 320

#define TFT_RGB_ORDER TFT_BGR
#define TFT_INVERSION_OFF
// If colors come out inverted, replace with: #define TFT_INVERSION_ON

// Display is on HSPI (SPI2) on this board.
#define USE_HSPI_PORT

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1     // tied to the ESP32 EN/RST line

// Backlight is handled in the shim (analogWrite on GPIO27), so do NOT let
// TFT_eSPI drive it. (No TFT_BL / TFT_BACKLIGHT_ON here on purpose.)

// Fonts the firmware uses (it prints with the GLCD font + printf at sizes
// 1-4). Load the common set so drawString/print all work.
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

#define SPI_FREQUENCY        40000000
#define SPI_READ_FREQUENCY   20000000
