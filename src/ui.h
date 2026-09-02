// Screens. Every function paints a whole frame into gfx; main.cpp flushes.
// Geometry is per board: the round 466 face and the 368x448 portrait.
#pragma once

#include <Arduino.h>

#include "tarot_engine.h"

static const uint16_t COL_BG = 0x0843;         // RGB(9, 8, 24) deep night
static const uint16_t COL_GOLD = 0xD58C;       // RGB(214,176,96)
static const uint16_t COL_GOLD_DIM = 0x7B27;   // RGB(120,100,60)
static const uint16_t COL_IVORY = 0xEF19;      // RGB(232,224,205)
static const uint16_t COL_DIM = 0x7B8D;        // RGB(122,114,104)
static const uint16_t COL_RULE = 0x3167;       // RGB(50,44,60)

struct Spread {
  Reading reading;
  bool revealed[3];
};

// Spread geometry, shared with hit testing in main.cpp.
#if defined(BOARD_ROUND_175)
#define UI_ROUND 1
static const int16_t SLOT_CX[3] = {99, 233, 367};
static const int16_t SLOT_Y = 122;
static const int16_t DECK_CX = 233, DECK_Y = 108, DECK_W = 150, DECK_H = 264;
#else
#define UI_ROUND 0
static const int16_t SLOT_CX[3] = {66, 184, 302};
static const int16_t SLOT_Y = 118;
static const int16_t DECK_CX = 184, DECK_Y = 118, DECK_W = 150, DECK_H = 264;
#endif

void uiBoot(uint32_t ageMs, bool fsOk, bool touchOk);
void uiDeck(uint32_t nowMs, bool holding, float progress);
void uiHelp();
void uiCut(float p);
void uiDeal(float p);
// The reverse: the three cards fly back into the stack, face down.
void uiGather(float p);
// flipping: slot index mid-flip or -1; flipPhase 0..1.
void uiSpread(const Spread &s, int8_t flipping, float flipPhase);
// One card as large as the glass allows, then its meaning as centered text.
void uiCardBig(const Spread &s, uint8_t pos);
void uiMeaning(const Spread &s, uint8_t pos);
// Lays out the composed text (mutated in place). Returns page count.
uint8_t uiInnerPrepare(char *text);
void uiInner(const Spread &s, uint8_t page, uint8_t pages);

bool uiDeckHit(int16_t x, int16_t y);
int8_t uiSlotHit(int16_t x, int16_t y);
