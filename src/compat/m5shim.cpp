// M5StickC Plus -> ESP32-2432S024R (CYD) shim implementation.

#include <M5StickCPlus.h>          // resolves to ../compat/M5StickCPlus.h (shim)
#include <SPI.h>
#include <time.h>

M5Shim M5;

// ── Touch (XPT2046) ─────────────────────────────────────────────────────
// The XPT2046 shares the display's HSPI pins (SCLK 14 / MOSI 13 / MISO 12),
// differing only by CS (33) and IRQ (36). Rather than depend on a touch
// library (whose pinned versions hardwire the global VSPI bus), we drive the
// controller directly over our own SPIClass(HSPI) handle. The protocol is a
// 3-byte command per axis; we read X, Y, and Z (pressure).
static SPIClass g_touchSpi(HSPI);
static const SPISettings TOUCH_SPI(2500000, MSBFIRST, SPI_MODE0);

static bool     g_touchDown = false;
static uint16_t g_tx = 0, g_ty = 0;
static bool     g_barDirty = true;

// XPT2046 control bytes: start(1) | A2A1A0 | mode(0=12bit) | SER/DFR(0) | PD.
#define XPT_CMD_X 0xD0   // 1101 0000 — X position
#define XPT_CMD_Y 0x90   // 1001 0000 — Y position
#define XPT_CMD_Z1 0xB0  // 1011 0000 — Z1
#define XPT_CMD_Z2 0xC0  // 1100 0000 — Z2

static uint16_t xptRead(uint8_t cmd) {
  g_touchSpi.transfer(cmd);
  uint8_t hi = g_touchSpi.transfer(0x00);
  uint8_t lo = g_touchSpi.transfer(0x00);
  return ((hi << 8) | lo) >> 3;   // 12-bit result, left-justified
}

// Raw sample from the controller. Returns false if not pressed hard enough.
static bool xptSample(uint16_t& rawX, uint16_t& rawY, uint16_t& z) {
  g_touchSpi.beginTransaction(TOUCH_SPI);
  digitalWrite(S024_TOUCH_CS, LOW);
  uint16_t z1 = xptRead(XPT_CMD_Z1);
  uint16_t z2 = xptRead(XPT_CMD_Z2);
  int zv = (int)z1 + 4095 - (int)z2;   // higher = harder press
  uint16_t x = 0, y = 0;
  if (zv >= S024_TOUCH_Z_MIN) {
    // A couple of reads, keep the last (settled) value.
    x = xptRead(XPT_CMD_X);
    x = xptRead(XPT_CMD_X);
    y = xptRead(XPT_CMD_Y);
    y = xptRead(XPT_CMD_Y);
  }
  digitalWrite(S024_TOUCH_CS, HIGH);
  g_touchSpi.endTransaction();
  rawX = x; rawY = y; z = (uint16_t)(zv < 0 ? 0 : zv);
  return zv >= S024_TOUCH_Z_MIN && x > 0 && y > 0;
}

// Map a raw XPT2046 sample to screen pixels, applying swap/invert/calibration.
static void mapTouch(uint16_t rawX, uint16_t rawY, uint16_t& sx, uint16_t& sy) {
  long x = rawX, y = rawY;
#if S024_TOUCH_SWAP_XY
  { long t = x; x = y; y = t; }
#endif
  x = (long)(x - S024_TOUCH_RAW_MIN_X) * (S024_TFT_W - 1) /
      (S024_TOUCH_RAW_MAX_X - S024_TOUCH_RAW_MIN_X);
  y = (long)(y - S024_TOUCH_RAW_MIN_Y) * (S024_TFT_H - 1) /
      (S024_TOUCH_RAW_MAX_Y - S024_TOUCH_RAW_MIN_Y);
  if (x < 0) x = 0; if (x > S024_TFT_W - 1) x = S024_TFT_W - 1;
  if (y < 0) y = 0; if (y > S024_TFT_H - 1) y = S024_TFT_H - 1;
#if S024_TOUCH_INVERT_X
  x = (S024_TFT_W - 1) - x;
#endif
#if S024_TOUCH_INVERT_Y
  y = (S024_TFT_H - 1) - y;
#endif
  sx = (uint16_t)x; sy = (uint16_t)y;
}

static bool readTouch(uint16_t& sx, uint16_t& sy) {
  uint16_t rawX, rawY, z;
  if (!xptSample(rawX, rawY, z)) return false;
  mapTouch(rawX, rawY, sx, sy);
#if S024_TOUCH_DEBUG
  Serial.printf("touch raw=(%u,%u) z=%u -> (%u,%u)\n", rawX, rawY, z, sx, sy);
#endif
  return true;
}

// ── M5 lifecycle ───────────────────────────────────────────────────────
void M5Shim::begin() {
  // Backlight on first so a slow display init isn't a black mystery.
  pinMode(S024_BL_PIN, OUTPUT);
  digitalWrite(S024_BL_PIN, HIGH);

  // RGB LED off (active-low → HIGH = off).
  pinMode(S024_LED_R, OUTPUT); digitalWrite(S024_LED_R, HIGH);
  pinMode(S024_LED_G, OUTPUT); digitalWrite(S024_LED_G, HIGH);
  pinMode(S024_LED_B, OUTPUT); digitalWrite(S024_LED_B, HIGH);

  Lcd.init();
  Lcd.setRotation(0);
  Lcd.invertDisplay(S024_INVERT_DISPLAY);

#if S024_BOOT_SELFTEST
  Serial.println("[s024] display self-test: R/G/B");
  Lcd.fillScreen(TFT_RED);   delay(400);
  Lcd.fillScreen(TFT_GREEN); delay(400);
  Lcd.fillScreen(TFT_BLUE);  delay(400);
#endif

  Lcd.fillScreen(TFT_BLACK);
  Axp.ScreenBreath(80);

  // Touch on its own HSPI handle, sharing the display SCLK/MOSI/MISO pins.
  pinMode(S024_TOUCH_CS, OUTPUT);
  digitalWrite(S024_TOUCH_CS, HIGH);
  pinMode(S024_TOUCH_IRQ, INPUT);
  g_touchSpi.begin(S024_TOUCH_SCLK, S024_TOUCH_MISO, S024_TOUCH_MOSI, S024_TOUCH_CS);
}

void M5Shim::update() {
  g_touchDown = readTouch(g_tx, g_ty);

  // Bottom bar splits into A (left half) / B (right half).
  bool aDown = false, bDown = false;
  if (g_touchDown && g_ty >= S024_BAR_Y) {
    if (g_tx < S024_TFT_W / 2) aDown = true;
    else                       bDown = true;
  }
  BtnA.set(aDown);
  BtnB.set(bDown);
}

bool m5TouchAnyDown() { return g_touchDown; }
void m5SoftButtonsInvalidate() { g_barDirty = true; }

// ── Backlight (analogWrite PWM on GPIO27, active-high) ──────────────────
void ShimAxp::ScreenBreath(uint8_t pct) {
  if (pct > 100) pct = 100;
  _lastPct = pct;
  analogWrite(S024_BL_PIN, (int)((uint32_t)pct * S024_BL_FULL / 100));
}
void ShimAxp::SetLDO2(bool on) {
  analogWrite(S024_BL_PIN, on ? (int)((uint32_t)_lastPct * S024_BL_FULL / 100) : 0);
}
void ShimAxp::PowerOff() {
  // No PMIC to cut power; go dark and idle until a tap, then reboot.
  analogWrite(S024_BL_PIN, 0);
  uint16_t x, y;
  while (!readTouch(x, y)) delay(50);
  ESP.restart();
}

// ── RGB LED (active-low) ────────────────────────────────────────────────
void m5SetLed(bool r, bool g, bool b) {
  digitalWrite(S024_LED_R, r ? LOW : HIGH);
  digitalWrite(S024_LED_G, g ? LOW : HIGH);
  digitalWrite(S024_LED_B, b ? LOW : HIGH);
}

// ── Software RTC ────────────────────────────────────────────────────────
static int64_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  int era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = (unsigned)(y - era * 400);
  unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return (int64_t)era * 146097 + (int)doe - 719468;
}

void ShimRtc::recompute() {
  if (!(_haveTime && _haveDate)) return;
  // The bridge already applied the tz offset, so treat components as UTC.
  int64_t days = daysFromCivil(_d.Year, _d.Month, _d.Date);
  _baseEpoch = (time_t)(days * 86400 + _t.Hours * 3600 + _t.Minutes * 60 + _t.Seconds);
  _baseMs = millis();
}
void ShimRtc::SetTime(RTC_TimeTypeDef* t) { _t = *t; _haveTime = true; recompute(); }
void ShimRtc::SetDate(RTC_DateTypeDef* d) { _d = *d; _haveDate = true; recompute(); }

void ShimRtc::GetTime(RTC_TimeTypeDef* t) {
  if (!_baseEpoch) { *t = _t; return; }
  time_t now = _baseEpoch + (millis() - _baseMs) / 1000;
  struct tm lt; gmtime_r(&now, &lt);
  t->Hours = lt.tm_hour; t->Minutes = lt.tm_min; t->Seconds = lt.tm_sec;
}
void ShimRtc::GetDate(RTC_DateTypeDef* d) {
  if (!_baseEpoch) { *d = _d; return; }
  time_t now = _baseEpoch + (millis() - _baseMs) / 1000;
  struct tm lt; gmtime_r(&now, &lt);
  d->WeekDay = lt.tm_wday; d->Month = lt.tm_mon + 1;
  d->Date = lt.tm_mday; d->Year = lt.tm_year + 1900;
}

// ── Scaled sprite push ──────────────────────────────────────────────────
// The app renders a 135x240 sprite. Nearest-neighbour upscale it into a RAM
// buffer (no PSRAM on this board, but the target is small) and blit it into
// the panel above the button bar.
void m5PushBuddy(TFT_eSprite& spr) {
  const int sw = spr.width(), sh = spr.height();
  const int ow = (int)(sw * S024_SPR_ZOOM_X + 0.5f);
  const int oh = (int)(sh * S024_SPR_ZOOM_Y + 0.5f);

  static uint16_t* buf = nullptr;
  static int cap = 0;
  if (!buf || cap < ow * oh) {
    if (buf) free(buf);
    cap = ow * oh;
    buf = (uint16_t*)malloc((size_t)cap * sizeof(uint16_t));
  }
  uint16_t* src = (uint16_t*)spr.getPointer();
  if (!buf || !src) { spr.pushSprite(0, 0); return; }

  for (int oy = 0; oy < oh; oy++) {
    int sy = (int)(oy / S024_SPR_ZOOM_Y); if (sy >= sh) sy = sh - 1;
    const uint16_t* srow = src + sy * sw;
    uint16_t* drow = buf + oy * ow;
    for (int ox = 0; ox < ow; ox++) {
      int sx = (int)(ox / S024_SPR_ZOOM_X); if (sx >= sw) sx = sw - 1;
      drow[ox] = srow[sx];
    }
  }

  int x = S024_SPR_CX - ow / 2;
  int y = S024_SPR_CY - oh / 2;
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  bool prevSwap = M5.Lcd.getSwapBytes();
  M5.Lcd.setSwapBytes(true);
  M5.Lcd.pushImage(x, y, ow, oh, buf);
  M5.Lcd.setSwapBytes(prevSwap);
}

// ── Soft A/B buttons ────────────────────────────────────────────────────
void m5DrawSoftButtons() {
  TFT_eSPI& g = M5.Lcd;
  const int y  = S024_BAR_Y;
  const int h  = S024_BAR_H - 2;
  const int wL = S024_TFT_W / 2;
  bool aHot = M5.BtnA.isPressed();
  bool bHot = M5.BtnB.isPressed();

  static bool lastA = false, lastB = false;
  if (!g_barDirty && aHot == lastA && bHot == lastB) return;
  g_barDirty = false;
  lastA = aHot; lastB = bHot;

  g.drawFastHLine(0, y - 1, S024_TFT_W, TFT_DARKGREY);

  // A = approve (green), B = deny (red).
  g.fillRect(0, y, wL - 1, h, aHot ? GREEN : TFT_BLACK);
  g.drawRect(0, y, wL - 1, h, GREEN);
  g.fillRect(wL + 1, y, S024_TFT_W - wL - 1, h, bHot ? RED : TFT_BLACK);
  g.drawRect(wL + 1, y, S024_TFT_W - wL - 1, h, RED);

  g.setTextDatum(MC_DATUM);
  g.setTextSize(3);
  g.setTextColor(aHot ? TFT_BLACK : GREEN, aHot ? GREEN : TFT_BLACK);
  g.drawString("A", wL / 2, y + h / 2);
  g.setTextColor(bHot ? TFT_BLACK : RED, bHot ? RED : TFT_BLACK);
  g.drawString("B", wL + (S024_TFT_W - wL) / 2, y + h / 2);
  g.setTextDatum(TL_DATUM);
  g.setTextSize(1);
}
