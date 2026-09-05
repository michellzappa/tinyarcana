// Screens. Every function paints a whole frame into gfx; main.cpp flushes.
// Geometry targets the 466px round face.
#pragma once

#include <Arduino.h>

#include "tarot_engine.h"
#include "settings.h"

static const uint16_t COL_BG = 0x0000;         // true black: OLED pixels off
static const uint16_t COL_GOLD = 0xD58C;       // RGB(214,176,96)
static const uint16_t COL_GOLD_DIM = 0x7B27;   // RGB(120,100,60)
static const uint16_t COL_IVORY = 0xEF19;      // RGB(232,224,205)
static const uint16_t COL_DIM = 0x7B8D;        // RGB(122,114,104)
static const uint16_t COL_RULE = 0x3167;       // RGB(50,44,60)

struct Spread {
  Reading reading;
  uint8_t deck;
  bool revealed[3];
  // 3 for past / present / future, 1 for a single card. Held on the spread
  // rather than read from settings, so a reading in progress keeps its shape
  // if the setting changes underneath it.
  uint8_t count;
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
// A single card sits at index 0 but shows in the middle slot and reads as the
// present: a one-card draw is about now, not about the past.
static inline int16_t slotCx(uint8_t count, uint8_t i) { return SLOT_CX[count == 1 ? 1 : i]; }
static inline uint8_t slotTextPos(uint8_t count, uint8_t i) { return count == 1 ? 1 : i; }

void uiBoot(uint32_t ageMs, bool fsOk, bool touchOk);
void uiDeck(uint32_t nowMs, bool holding, float progress);
void uiMenu(uint8_t selected);
void uiHelp();
void uiSettings(const AppSettings &settings, uint8_t selected);
void uiDeal(float p, uint8_t count);
// The reverse: the three cards fly back into the stack, face down.
void uiGather(float p, uint8_t count);
// flipping: slot index mid-flip or -1; flipPhase 0..1.
void uiSpread(const Spread &s, int8_t flipping, float flipPhase);
// The tapped card grows from its slot to fill the glass; p 0..1.
void uiZoom(const Spread &s, uint8_t pos, float p);
// One card as large as the glass allows, then its meaning as centered text.
void uiCardBig(const Spread &s, uint8_t pos);
void uiMeaning(const Spread &s, uint8_t pos);
// Lays out the composed text (mutated in place). Returns page count.
uint8_t uiInnerPrepare(char *text);
void uiInner(const Spread &s, uint8_t page, uint8_t pages);

bool uiDeckHit(int16_t x, int16_t y);
int8_t uiMenuHit(int16_t x, int16_t y);
int8_t uiSettingsHit(int16_t x, int16_t y);
int8_t uiSlotHit(const Spread &s, int16_t x, int16_t y);
