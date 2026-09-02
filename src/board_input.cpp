// Buttons and touch per board.
//   round 1.75: CST9217 via the write-then-ACK protocol ported in
//               esp32-lofiair (SensorLib's TouchDrvCST92xx, line for line).
//               Bus at 100 kHz and interrupt-gated reads, both as LoFi Air
//               ships them. PWR is the AXP2101 key IRQ.
//   1.8:        FT3168 register reads, PWR via the TCA9554 input register.
#include "board_input.h"

#include <Wire.h>
#include <string.h>

#include "board_display.h"
#include "pin_config.h"

static const uint32_t DEBOUNCE_MS = 40;
static const uint32_t LONG_MS = 800;
static const int16_t TAP_MAX_PX = 24;
static const int16_t SWIPE_MIN_PX = 56;

static uint8_t touchAddr = 0;
static volatile bool touchIrq = false;

static bool bootWas = false;
static uint32_t bootEdgeMs = 0, bootDownMs = 0;
static bool bootLongFired = false;

static bool fingerDown = false;
static int16_t startX = 0, startY = 0, lastX = 0, lastY = 0;
static uint32_t fingerDownMs = 0;

static void IRAM_ATTR onTouchIrq() { touchIrq = true; }

// ---------------- Touch: CST9217 (round) ----------------
#if defined(BOARD_ROUND_175)

static bool cstWriteThenRead(const uint8_t *w, size_t wn, uint8_t *r, size_t rn) {
  Wire.beginTransmission(touchAddr);
  Wire.write(w, wn);
  // STOP between command and response. A repeated start returned stale 0x08
  // status packets on LoFi Air's unit and every touch failed the ACK check.
  if (Wire.endTransmission(true) != 0) return false;
  if (rn == 0) return true;
  if (Wire.requestFrom((int)touchAddr, (int)rn) != (int)rn) return false;
  for (size_t i = 0; i < rn; i++) r[i] = Wire.read();
  return true;
}

static bool cstWrite(const uint8_t *w, size_t wn) {
  Wire.beginTransmission(touchAddr);
  Wire.write(w, wn);
  return Wire.endTransmission() == 0;
}

// TouchDrvCST92xx::getAttribute(): reset, read checkcode / resolution /
// chip type / firmware, validate. No mode switch; the chip reports points
// after this, as in the vendor demo.
static bool cst9217Attach() {
  pinMode(TP_RST, OUTPUT);
  digitalWrite(TP_RST, LOW);
  delay(10);
  digitalWrite(TP_RST, HIGH);
  delay(30);

  uint8_t buf[8];
  const uint8_t enterCmd[2] = {0xD1, 0x01};
  if (!cstWrite(enterCmd, 2)) return false;
  delay(10);

  const uint8_t rCheck[2] = {0xD1, 0xFC};
  if (!cstWriteThenRead(rCheck, 2, buf, 4)) return false;
  const uint32_t checkcode = ((uint32_t)buf[3] << 24) | ((uint32_t)buf[2] << 16) |
                             ((uint32_t)buf[1] << 8) | buf[0];

  const uint8_t rRes[2] = {0xD1, 0xF8};
  if (!cstWriteThenRead(rRes, 2, buf, 4)) return false;
  const uint16_t resX = (uint16_t)((buf[1] << 8) | buf[0]);
  const uint16_t resY = (uint16_t)((buf[3] << 8) | buf[2]);

  const uint8_t rType[2] = {0xD2, 0x04};
  if (!cstWriteThenRead(rType, 2, buf, 4)) return false;
  const uint16_t chipType = (uint16_t)((buf[3] << 8) | buf[2]);

  const uint8_t rVer[2] = {0xD2, 0x08};
  if (!cstWriteThenRead(rVer, 2, buf, 8)) return false;
  const uint32_t fwVersion = ((uint32_t)buf[3] << 24) | ((uint32_t)buf[2] << 16) |
                             ((uint32_t)buf[1] << 8) | buf[0];

  Serial.printf("touch @ 0x%02X CST9217 checkcode=0x%lX res=%ux%u type=0x%04X fw=0x%08lX\n",
                touchAddr, (unsigned long)checkcode, resX, resY, chipType,
                (unsigned long)fwVersion);
  if (fwVersion == 0xA5A5A5A5) { Serial.println("touch: CST9217 has no firmware"); return false; }
  if ((checkcode & 0xFFFF0000UL) != 0xCACA0000UL) { Serial.println("touch: checkcode mismatch"); return false; }
  if (chipType != 0x9220 && chipType != 0x9217) { Serial.printf("touch: chip type 0x%04X unexpected\n", chipType); return false; }
  return true;
}

// TouchDrvCST92xx::getPoint(), first finger. Contact and release come back
// through the same read/ack transaction: no valid point means no finger.
static bool cst9217Poll(int16_t *x, int16_t *y) {
  uint8_t buf[15];
  const uint8_t readCmd[2] = {0xD0, 0x00};
  if (!cstWriteThenRead(readCmd, 2, buf, sizeof buf)) return false;
  const uint8_t ackCmd[3] = {0xD0, 0x00, 0xAB};
  if (!cstWrite(ackCmd, 3) || buf[6] != 0xAB) return false;
  const uint8_t numPoints = buf[5] & 0x7F;
  const uint8_t id = buf[0] >> 4;
  const uint8_t evt = buf[0] & 0x0F;
  if (numPoints == 0 || numPoints > 2 || id >= 2 || evt != 0x06) return false;
  *x = (int16_t)(((uint16_t)buf[1] << 4) | (buf[3] >> 4));
  *y = (int16_t)(((uint16_t)buf[2] << 4) | (buf[3] & 0x0F));
  return true;
}

// Both axes mirrored: Waveshare's example calls setMirrorXY(true, true) for
// this board (LoFi Air and Headroom both ship this mapping).
// The mirrored panel point is then mapped through the same rotation the
// display applies on flush (board_display.cpp).
static void mirror(int16_t nx, int16_t ny, int16_t *x, int16_t *y) {
  nx = (int16_t)constrain(nx, 0, SCR_W - 1);
  ny = (int16_t)constrain(ny, 0, SCR_H - 1);
  nativeToLogical((int16_t)(SCR_W - 1 - nx), (int16_t)(SCR_H - 1 - ny), x, y);
}

// LoFi Air's read discipline, kept exactly: read a packet only after the
// controller's falling-edge interrupt. Polling every frame consumed and
// ACKed idle packets and made single taps look like they needed two.
// Some firmware revisions send no distinct release packet, so a finger is
// considered lifted after a quiet period with no new point.
static const uint32_t TOUCH_RELEASE_QUIET_MS = 120;
static uint32_t touchLastPointMs = 0;

// Returns: 1 = point, 0 = nothing new this frame, -1 = released.
static int8_t touchSample(int16_t *x, int16_t *y, uint32_t now) {
  if (!touchAddr) return 0;
  bool ready;
  noInterrupts();
  ready = touchIrq;
  touchIrq = false;
  interrupts();
  if (!ready) {
    if (fingerDown && now - touchLastPointMs >= TOUCH_RELEASE_QUIET_MS) return -1;
    return 0;
  }
  int16_t nx = 0, ny = 0;
  if (!cst9217Poll(&nx, &ny)) {
    // A packet with no finger in it is the release, when the chip sends one.
    return fingerDown ? -1 : 0;
  }
  mirror(nx, ny, x, y);
  touchLastPointMs = now;
  return 1;
}

static void touchInit() {
  // Waveshare's exact-board example leaves Wire at 100 kHz. At 400 kHz the
  // 15-byte point packets came back with a corrupt ACK on LoFi Air's unit.
  Wire.setClock(100000);
  touchAddr = CST9217_ADDR;
  if (!i2cPresent(touchAddr) || !cst9217Attach()) {
    Serial.println("touch: none");
    touchAddr = 0;
    return;
  }
  pinMode(TP_INT, INPUT_PULLUP);
  touchIrq = false;
  attachInterrupt(digitalPinToInterrupt(TP_INT), onTouchIrq, FALLING);
}

// ---------------- Touch: FT3168 / CST816 (1.8) ----------------
#else

static bool touchWrite8(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(touchAddr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

static bool touchRead(uint8_t reg, uint8_t *buf, uint8_t n) {
  Wire.beginTransmission(touchAddr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)touchAddr, (int)n) != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

// One transaction: touch count + first point, laid out the same on both chips.
static bool touchPoint(int16_t *x, int16_t *y) {
  if (!touchAddr) return false;
  uint8_t b[5];
  if (!touchRead(0x02, b, 5)) return false;
  if ((b[0] & 0x0F) == 0) return false;
  *x = (int16_t)(((uint16_t)(b[1] & 0x0F) << 8) | b[2]);
  *y = (int16_t)(((uint16_t)(b[3] & 0x0F) << 8) | b[4]);
  *x = (int16_t)constrain(*x, 0, SCR_W - 1);
  *y = (int16_t)constrain(*y, 0, SCR_H - 1);
  return true;
}

static int8_t touchSample(int16_t *x, int16_t *y, uint32_t now) {
  (void)now;
  if (!touchAddr) return 0;
  if (touchPoint(x, y)) return 1;
  return fingerDown ? -1 : 0;
}

static void touchInit() {
  pinMode(TP_INT, INPUT_PULLUP);
  if (i2cPresent(FT3168_ADDR)) {
    touchAddr = FT3168_ADDR;
    touchWrite8(0x00, 0x00);   // active mode; monitor mode reports no coords
    touchWrite8(0xA5, 0x00);
    delay(20);
  } else if (i2cPresent(CST816_ADDR)) {
    touchAddr = CST816_ADDR;
    touchWrite8(0xFA, 0x40);
    delay(20);
  } else {
    Serial.println("touch: none");
    return;
  }
  Serial.printf("touch @ 0x%02X\n", touchAddr);
}

#endif

bool boardTouchPresent() { return touchAddr != 0; }

void boardInputBegin() {
  pinMode(BTN_BOOT, INPUT_PULLUP);
  touchInit();
}

// ---------------- PWR ----------------
// Returns true once per short press.
static bool pwrShortPressed(uint32_t now) {
#if defined(BOARD_ROUND_175)
  // PKEY IRQs persist until cleared; human-button cadence is plenty.
  static uint32_t lastPoll = 0;
  if (!pmuOk || now - lastPoll < 25) return false;
  lastPoll = now;
  PMU.getIrqStatus();
  const bool shortPress = PMU.isPekeyShortPressIrq();
  PMU.clearIrqStatus();
  return shortPress;
#else
  static bool pwrWas = false;
  static uint32_t pwrDownMs = 0, pwrPollMs = 0;
  if (now - pwrPollMs < 20) return false;
  pwrPollMs = now;
  Wire.beginTransmission(tcaAddress());
  Wire.write(0x00);
  bool down = false;
  if (Wire.endTransmission(false) == 0 && Wire.requestFrom((int)tcaAddress(), 1) == 1)
    down = (Wire.read() & (1u << BTN_PWR_TCA_BIT)) != 0;
  bool pressed = false;
  if (down && !pwrWas) pwrDownMs = now;
  if (!down && pwrWas) {
    const uint32_t held = now - pwrDownMs;
    pressed = held >= DEBOUNCE_MS && held < LONG_MS;
  }
  pwrWas = down;
  return pressed;
#endif
}

// ---------------- Poll ----------------
void boardInputPoll(InputFrame *out) {
  memset(out, 0, sizeof(*out));
  const uint32_t now = millis();

  // --- BOOT ---
  // A short press reports on release, so a BOOT held for the BOOT+PWR chord
  // or for the long hold does not also fire the short action on the way in.
  const bool bootDown = digitalRead(BTN_BOOT) == LOW;
  if (bootDown != bootWas && (now - bootEdgeMs) >= DEBOUNCE_MS) {
    bootEdgeMs = now;
    bootWas = bootDown;
    if (bootDown) {
      bootDownMs = now;
      bootLongFired = false;
    } else if (now - bootDownMs < LONG_MS) {
      out->aPressed = true;
    }
  }
  if (bootWas && !bootLongFired && (now - bootDownMs) >= LONG_MS) {
    bootLongFired = true;
    out->aLong = true;
  }
  out->aDown = bootWas;

  // --- PWR ---
  out->bPressed = pwrShortPressed(now);

  // --- Touch ---
  int16_t x = 0, y = 0;
  const int8_t sample = touchSample(&x, &y, now);
  if (sample > 0) {
    if (!fingerDown) {
      fingerDown = true;
      fingerDownMs = now;
      startX = x;
      startY = y;
      out->touchBegan = true;
    }
    lastX = x;
    lastY = y;
  } else if (sample < 0 && fingerDown) {
    fingerDown = false;
    out->touchEnded = true;
    const int16_t dx = (int16_t)(lastX - startX);
    const int16_t dy = (int16_t)(lastY - startY);
    const int16_t adx = dx < 0 ? (int16_t)-dx : dx;
    const int16_t ady = dy < 0 ? (int16_t)-dy : dy;
    const uint32_t held = now - fingerDownMs;
    if (adx >= SWIPE_MIN_PX && adx > ady * 2 && held < 900) {
      if (dx < 0) out->swipeLeft = true;
      else out->swipeRight = true;
    } else if (adx <= TAP_MAX_PX && ady <= TAP_MAX_PX && held < 600) {
      out->tap = true;
    }
  }
  out->touchDown = fingerDown;
  out->x = lastX;
  out->y = lastY;
  out->startX = startX;
  out->startY = startY;
  out->holdMs = fingerDown ? (now - fingerDownMs) : 0;
}
