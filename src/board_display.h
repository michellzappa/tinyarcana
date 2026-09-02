#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

#define XPOWERS_CHIP_AXP2101
#include <XPowersLib.h>

#include "pin_config.h"

// Native orientation on both boards: logical == panel, no rotation on flush.
static const int16_t SCR_W = LCD_WIDTH;
static const int16_t SCR_H = LCD_HEIGHT;

class PortraitCanvas : public Arduino_Canvas {
  uint16_t _bg = 0;
  uint16_t *_native = nullptr;   // rotated copy, round board only

public:
  PortraitCanvas(Arduino_G *out);
  bool begin(int32_t speed = GFX_NOT_DEFINED) override;
  uint16_t *fb() { return _framebuffer; }
  void clear(uint16_t color);
  void flush(bool force_flush = false) override;
};

extern PortraitCanvas *gfx;
extern XPowersPMU PMU;
extern bool pmuOk;

bool boardDisplayBegin();
// Panel (touch-controller) coordinates to the coordinates the UI draws in.
void nativeToLogical(int16_t nx, int16_t ny, int16_t *lx, int16_t *ly);
void boardSetBrightness(uint8_t b);
bool i2cPresent(uint8_t addr);
uint8_t tcaAddress();   // 1.8 only; 0 on boards without an expander
