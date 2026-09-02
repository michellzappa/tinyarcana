// Card bitmaps from LittleFS (data/<env>/cards/NN_WxH.565), cached in PSRAM,
// blitted with rounded corners and an optional horizontal squash for flips.
#pragma once

#include <Arduino.h>

// S: the three-card spread. L: one card, as large as the glass allows.
enum CardSize : uint8_t { CARD_S = 0, CARD_L = 1 };

// Must match scripts/build_assets.py CARD_SIZES for the env.
#if defined(BOARD_ROUND_175)
// L is the largest 0.569 rectangle inside the 233 px circle, less a margin.
static const int16_t CARD_W[2] = {120, 224};
static const int16_t CARD_H[2] = {211, 394};
#else
static const int16_t CARD_W[2] = {104, 250};
static const int16_t CARD_H[2] = {183, 440};
#endif

bool cardsBegin();
bool cardsReady();
const uint16_t *cardBitmap(uint8_t idx, CardSize sz);
void cardPreload(uint8_t idx);

// Face: width scaled by squash (0..1) about cx; top edge at y.
void cardDrawFace(uint8_t idx, CardSize sz, int16_t cx, int16_t y, float squash);
// Face resampled from the large bitmap to any w x h (zoom animation).
void cardDrawFaceScaled(uint8_t idx, int16_t cx, int16_t y, int16_t w, int16_t h);
// Procedural back at any size. glow 0..1 lifts the gold.
void cardDrawBack(int16_t cx, int16_t y, int16_t w, int16_t h, float squash,
                  float glow);
