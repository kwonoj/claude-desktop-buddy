#pragma once
// M5StickC Plus -> ESP32-2432S024R (CYD) compatibility shim.
//
// This file is named M5StickCPlus.h ON PURPOSE. With `-I src/compat` on the
// include path (and the real m5stack/M5StickCPlus library absent from this
// env's lib_deps), every `#include <M5StickCPlus.h>` in the firmware resolves
// HERE instead, so the entire app builds unchanged against the CYD.
//
// Mapping:
//   M5.Lcd        -> a real TFT_eSPI instance (ST7789 240x320, HSPI)
//   TFT_eSprite   -> straight from TFT_eSPI (unchanged drawing API)
//   M5.BtnA/.BtnB -> virtual buttons driven by XPT2046 resistive touch zones
//   M5.Axp        -> backlight PWM (GPIO27) + faked battery/USB telemetry
//   M5.Imu        -> flat/no-motion stub (board has no IMU)
//   M5.Beep       -> no-op (no buzzer)
//   M5.Rtc        -> software RTC fed by the desktop's {"time":[...]} message

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <FS.h>
#include <LittleFS.h>
using fs::File;            // the real M5 header re-exported this
#include "s024_config.h"

// Colors the firmware references by bare name.
#ifndef GREEN
#define GREEN 0x07E0
#endif
#ifndef RED
#define RED 0xF800
#endif

// RTC structs — same field names/order the firmware initialises.
struct RTC_TimeTypeDef { uint8_t Hours, Minutes, Seconds; };
struct RTC_DateTypeDef { uint8_t WeekDay, Month, Date; uint16_t Year; };

// Virtual button: state is pushed each M5.update() from the touch zones.
class ShimButton {
public:
  void set(bool nowDown) {
    _wasPressed  = nowDown && !_down;
    _wasReleased = !nowDown && _down;
    if (_wasPressed) _pressStart = millis();
    _down = nowDown;
  }
  bool isPressed()  const { return _down; }
  bool wasPressed() const { return _wasPressed; }
  bool wasReleased() const { return _wasReleased; }
  bool pressedFor(uint32_t ms) const {
    return _down && (millis() - _pressStart) >= ms;
  }
private:
  bool _down = false, _wasPressed = false, _wasReleased = false;
  uint32_t _pressStart = 0;
};

// Power/brightness. The CYD has no PMIC or fuel gauge, so battery/USB/temp
// reads are faked: report a healthy USB-powered device. This keeps the
// upstream "clock face while on USB" path and the status command working.
class ShimAxp {
public:
  void ScreenBreath(uint8_t pct);   // backlight PWM
  void SetLDO2(bool on);            // screen on/off (restore vs. 0)
  void PowerOff();                  // backlight off, wait for tap, restart
  float GetBatVoltage()    { return 4.15f; }
  float GetBatCurrent()    { return 0.0f; }
  float GetVBusVoltage()   { return 5.0f; }   // > 4.0 => "on USB"
  int   GetTempInAXP192()  { return 25; }
  uint8_t GetBtnPress()    { return 0; }       // no hardware power button
private:
  uint8_t _lastPct = 80;
};

// No IMU — always report sitting flat (gravity on +Z). Shake/face-down/
// clock-rotate gestures go dormant; the matching states still fire from BLE.
class ShimImu {
public:
  void Init() {}
  void getAccelData(float* ax, float* ay, float* az) { *ax = 0; *ay = 0; *az = 1.0f; }
};

// No buzzer.
class ShimBeep {
public:
  void begin() {}
  void update() {}
  void tone(uint16_t /*freq*/, uint16_t /*durMs*/) {}
};

// Software RTC, set from {"time":[epoch_sec, tz_offset_sec]} by the desktop.
class ShimRtc {
public:
  void SetTime(RTC_TimeTypeDef* t);
  void SetDate(RTC_DateTypeDef* d);
  void GetTime(RTC_TimeTypeDef* t);
  void GetDate(RTC_DateTypeDef* d);
private:
  void recompute();
  RTC_TimeTypeDef _t{};
  RTC_DateTypeDef _d{};
  time_t   _baseEpoch = 0;
  uint32_t _baseMs = 0;
  bool _haveTime = false, _haveDate = false;
};

// The faked M5 facade.
class M5Shim {
public:
  TFT_eSPI   Lcd;
  ShimButton BtnA;
  ShimButton BtnB;
  ShimAxp    Axp;
  ShimImu    Imu;
  ShimBeep   Beep;
  ShimRtc    Rtc;

  void begin();
  void update();
};

extern M5Shim M5;

// ── Helpers used by the CYD-specific bits of main.cpp / the shim itself ──

// Push the 135x240 buddy sprite, scaled up, into the top of the panel.
void m5PushBuddy(TFT_eSprite& spr);

// Draw the two on-screen A/B touch targets in the bottom bar.
void m5DrawSoftButtons();

// True while a finger is anywhere on the panel (used for wake).
bool m5TouchAnyDown();

// Force the soft-button bar to repaint next frame (after a full clear).
void m5SoftButtonsInvalidate();

// Drive the onboard RGB LED (active-low). Used for the attention blink.
void m5SetLed(bool r, bool g, bool b);

// Frame presentation seam used by main.cpp's `BUDDY_PUSH(spr)`. On the CYD
// the 135x240 sprite is scaled into the top of the 240x320 panel and the
// soft A/B button bar is drawn underneath.
#define BUDDY_PUSH(spr) do { m5PushBuddy(spr); m5DrawSoftButtons(); } while (0)

// This board has no physical buttons — a tap anywhere should wake the screen.
#define BUDDY_TOUCH_WAKE 1

// The CYD panel is mounted landscape; render the whole UI rotated.
#define BUDDY_ROTATION S024_ROTATION
