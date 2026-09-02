// Panel bring-up per board, copied from firmware proven on each SKU.
//   round 1.75: esp32-lofiair board_display.cpp (LOFIAIR_AMOLED_175C)
//   1.8:        headroom/firmware and esp32-rhythm board_display.cpp
#include "board_display.h"

#include <Wire.h>
#include <esp_heap_caps.h>
#include <string.h>

XPowersPMU PMU;
bool pmuOk = false;

static Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);

#if defined(BOARD_ROUND_175)
// Waveshare's 01_HelloWorld config: CO5300, column offset 6, brightness via
// the panel. The AXP2101 rails are left at their power-on defaults; the
// vendor demos never configure them and the panel lights without it.
static Arduino_CO5300 *panel = new Arduino_CO5300(
    bus, LCD_RESET, 0 /* rotation */, LCD_WIDTH, LCD_HEIGHT,
    LCD_COL_OFFSET, 0, 0, 0);
#else
static Arduino_SH8601 *panel = new Arduino_SH8601(
    bus, -1 /* RST via expander */, 0, LCD_WIDTH, LCD_HEIGHT);
static uint8_t tcaAddr = TCA9554_ADDR;
#endif

uint8_t tcaAddress() {
#if defined(BOARD_ROUND_175)
  return 0;
#else
  return tcaAddr;
#endif
}

bool i2cPresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

static void i2cScan() {
  Serial.print("I2C:");
  int found = 0;
  for (uint8_t a = 1; a < 127; a++)
    if (i2cPresent(a)) { Serial.printf(" 0x%02X", a); found++; }
  Serial.println(found ? "" : " (none)");
}

// ---------------- Canvas ----------------
PortraitCanvas::PortraitCanvas(Arduino_G *out)
    : Arduino_Canvas(SCR_W, SCR_H, out, 0, 0, 0) {}

bool PortraitCanvas::begin(int32_t speed) {
  if ((speed != GFX_SKIP_OUTPUT_BEGIN) && _output) {
    if (!_output->begin(speed)) return false;
  }
  const size_t bytes = (size_t)SCR_W * SCR_H * sizeof(uint16_t);
  if (!_framebuffer) {
    _framebuffer = (uint16_t *)ps_malloc(bytes);
    if (!_framebuffer) return false;
    memset(_framebuffer, 0, bytes);
  }
  return true;
}

void PortraitCanvas::clear(uint16_t color) {
  if (!_framebuffer) return;
  _bg = color;
  uint32_t c32 = ((uint32_t)color << 16) | color;
  uint32_t *p = (uint32_t *)_framebuffer;
  size_t n = ((size_t)SCR_W * SCR_H) / 2;
  while (n--) *p++ = c32;
}

#if defined(BOARD_ROUND_175)

// The UI is drawn upright and rotated 90 degrees clockwise on the way to the
// panel: logical top lands on the panel's right. Done in tiles so the strided
// source walk stays cache-friendly (headroom's landscape rotate).
//   native(nx, ny) = logical(lx = ny, ly = SCR_H - 1 - nx)
static const int16_t ROT_TILE = 32;
static void rotateCW(const uint16_t *src, uint16_t *dst) {
  for (int16_t ny0 = 0; ny0 < SCR_H; ny0 += ROT_TILE) {
    const int16_t nyEnd = (int16_t)(ny0 + ROT_TILE < SCR_H ? ny0 + ROT_TILE : SCR_H);
    for (int16_t nx0 = 0; nx0 < SCR_W; nx0 += ROT_TILE) {
      const int16_t nxEnd = (int16_t)(nx0 + ROT_TILE < SCR_W ? nx0 + ROT_TILE : SCR_W);
      for (int16_t ny = ny0; ny < nyEnd; ny++) {
        uint16_t *d = dst + (int32_t)ny * SCR_W + nx0;
        // lx = ny (column), ly = SCR_H-1-nx (row): walk rows upward.
        const uint16_t *s = src + (int32_t)(SCR_H - 1 - nx0) * SCR_W + ny;
        for (int16_t nx = nx0; nx < nxEnd; nx++) {
          *d++ = *s;
          s -= SCR_W;
        }
      }
    }
  }
}

void nativeToLogical(int16_t nx, int16_t ny, int16_t *lx, int16_t *ly) {
  *lx = ny;
  *ly = (int16_t)(SCR_H - 1 - nx);
}

// One-shot full-frame blit; the CO5300 goes black on per-row QSPI writes.
void PortraitCanvas::flush(bool force_flush) {
  (void)force_flush;
  if (!_framebuffer || !_output) return;
  if (!_native) {
    _native = (uint16_t *)ps_malloc((size_t)SCR_W * SCR_H * sizeof(uint16_t));
    if (!_native) return;
  }
  rotateCW(_framebuffer, _native);
  _output->draw16bitRGBBitmap(0, 0, _native, SCR_W, SCR_H);
}

PortraitCanvas *gfx = new PortraitCanvas(panel);

void boardSetBrightness(uint8_t b) { panel->setBrightness(b); }

bool boardDisplayBegin() {
  Wire.begin(IIC_SDA, IIC_SCL, 400000);
  delay(50);
  i2cScan();

  // IRQ routing only, so the case's PWR key can be read; no rail writes.
  pmuOk = PMU.begin(Wire, AXP2101_ADDR, IIC_SDA, IIC_SCL);
  Serial.printf("AXP2101: %s\n", pmuOk ? "ok" : "FAIL");
  if (pmuOk) {
    PMU.enableBattDetection();
    PMU.enableBattVoltageMeasure();
    PMU.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    PMU.clearIrqStatus();
    // SHORT + LONG only. NEGATIVE/POSITIVE latched on a missed key-up in
    // LoFi Air and killed the button.
    PMU.enableIRQ(XPOWERS_AXP2101_PKEY_SHORT_IRQ |
                  XPOWERS_AXP2101_PKEY_LONG_IRQ);
  }

  const bool pok = panel->begin();
  Serial.printf("amoled panel begin: %s  psram=%d\n", pok ? "ok" : "FAIL",
                (int)psramFound());
  panel->setBrightness(200);

  const bool cok = gfx->begin(GFX_SKIP_OUTPUT_BEGIN);
  Serial.printf("canvas %dx%d: %s\n", SCR_W, SCR_H, cok ? "ok" : "FAIL");
  return pok && cok;
}

#else   // ---------------- 1.8 SH8601 ----------------

// Blank a few GRAM columns past LCD_WIDTH after every blit, then put the
// address window back so the driver's cached window matches the panel.
static void sealNativeEdges(uint16_t color) {
  const int16_t extra = 16;
  bus->beginWrite();
  bus->writeC8D16D16(0x2A, LCD_WIDTH, LCD_WIDTH + extra - 1);
  bus->writeC8D16D16(0x2B, 0, LCD_HEIGHT - 1);
  bus->writeCommand(0x2C);
  bus->writeRepeat(color, (uint32_t)extra * LCD_HEIGHT);
  bus->writeC8D16D16(0x2A, 0, LCD_WIDTH - 1);
  bus->writeC8D16D16(0x2B, 0, LCD_HEIGHT - 1);
  bus->endWrite();
}

void PortraitCanvas::flush(bool force_flush) {
  (void)force_flush;
  if (!_framebuffer || !_output) return;
  _output->draw16bitRGBBitmap(0, 0, _framebuffer, SCR_W, SCR_H);
  sealNativeEdges(_bg);
}

void nativeToLogical(int16_t nx, int16_t ny, int16_t *lx, int16_t *ly) {
  *lx = nx;
  *ly = ny;
}

PortraitCanvas *gfx = new PortraitCanvas(panel);

static void sh8601VendorInit() {
  bus->beginWrite();
  bus->writeCommand(0x11);
  bus->endWrite();
  delay(120);

  bus->beginWrite();
  bus->writeC8D8(0xFE, 0x20);
  bus->writeC8D8(0x19, 0x10);
  bus->writeC8D8(0x1C, 0xA0);
  bus->writeC8D8(0xFE, 0x00);
  bus->writeC8D8(0xC4, 0x80);
  bus->writeC8D8(0x3A, 0x55);
  bus->writeC8D8(0x35, 0x00);
  bus->writeC8D8(0x36, 0x00);
  bus->writeC8D8(0x53, 0x20);
  bus->writeC8D8(0x51, 0xFF);
  bus->writeC8D8(0x63, 0xFF);

  uint8_t col[4] = {0x00, 0x00, (uint8_t)((LCD_WIDTH - 1) >> 8),
                    (uint8_t)((LCD_WIDTH - 1) & 0xFF)};
  bus->writeCommand(0x2A);
  bus->writeBytes(col, 4);
  uint8_t row[4] = {0x00, 0x00, (uint8_t)((LCD_HEIGHT - 1) >> 8),
                    (uint8_t)((LCD_HEIGHT - 1) & 0xFF)};
  bus->writeCommand(0x2B);
  bus->writeBytes(row, 4);
  bus->writeCommand(0x29);
  bus->endWrite();
  delay(50);
}

static void tcaWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(tcaAddr);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

// Expander P0 = LCD reset, P1 = touch reset, P2 = DSI power enable. P4 (PWR
// key) and the rest stay inputs.
static void panelReset() {
  tcaWrite(0x03, 0xF8);
  tcaWrite(0x01, 0x00);
  delay(30);
  tcaWrite(0x01, 0x07);
  delay(120);
}

static void powerInit() {
  Wire.begin(IIC_SDA, IIC_SCL, 400000);
  delay(50);
  i2cScan();

  if (!i2cPresent(tcaAddr) && i2cPresent(0x21)) tcaAddr = 0x21;
  Serial.printf("TCA9554 @ 0x%02X %s\n", tcaAddr,
                i2cPresent(tcaAddr) ? "ok" : "NO ACK (panel stays dark)");

  pmuOk = PMU.begin(Wire, AXP2101_ADDR, IIC_SDA, IIC_SCL);
  Serial.printf("AXP2101: %s\n", pmuOk ? "ok" : "FAIL (display rail off)");
  if (pmuOk) {
    PMU.setALDO1Voltage(3300); PMU.enableALDO1();
    PMU.setALDO2Voltage(3300); PMU.enableALDO2();
    PMU.setALDO3Voltage(3300); PMU.enableALDO3();   // display rail
    PMU.setALDO4Voltage(3300); PMU.enableALDO4();
    PMU.enableBattDetection();
    PMU.enableBattVoltageMeasure();
    delay(50);
  }
  panelReset();
}

void boardSetBrightness(uint8_t b) { panel->setBrightness(b); }

bool boardDisplayBegin() {
  powerInit();

  bool pok = panel->begin();
  Serial.printf("panel begin: %s\n", pok ? "ok" : "FAIL");
  sh8601VendorInit();
  // Wipe GRAM, including the columns past LCD_WIDTH, before lighting up.
  panel->fillScreen(0);
  sealNativeEdges(0);
  panel->setBrightness(180);

  bool cok = gfx->begin(GFX_SKIP_OUTPUT_BEGIN);
  Serial.printf("canvas %dx%d: %s  psram=%d\n", SCR_W, SCR_H,
                cok ? "ok" : "FAIL", (int)psramFound());
  return pok && cok;
}

#endif
