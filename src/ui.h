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
// Deck, brightness, language, reset. Shared so the renderer, the hit test and
// the BOOT-button cursor in main.cpp cannot disagree.
//
// Draw (one card or three) is not here. It changes the ritual on the deck
// screen, so its control lives there, where the change is felt.
enum SettingsRow : uint8_t {
  SET_ROW_DECK = 0,
  SET_ROW_BRIGHT,
  SET_ROW_LANG,
  SET_ROW_RESET,
};
static const uint8_t SETTINGS_ROWS = 4;
// resetArmed draws the reset row asking for a second tap.
void uiSettings(const AppSettings &settings, uint8_t selected, bool resetArmed);
// Brightness is dragged along its row, so the x under the finger is the value.
// The geometry lives in ui.cpp; main.cpp asks it what an x means.
uint8_t uiBrightnessAtX(int16_t x);

// The way into Settings from the deck screen.
bool uiSettingsLinkHit(int16_t x, int16_t y);

// Closing the reading, from the spread screen. Armed on the first tap.
bool uiReadingResetHit(int16_t x, int16_t y);

// The one-card / three-card control on the deck screen. Both of these test a
// target larger than the pill that is drawn.
// Returns 0 for one card, 1 for three, -1 for a miss.
int8_t uiDrawModeHit(int16_t x, int16_t y);
void uiDeal(float p, uint8_t count);
// The reverse: the three cards fly back into the stack, face down.
void uiGather(float p, uint8_t count);
// flipping: slot index mid-flip or -1; flipPhase 0..1.
void uiSpread(const Spread &s, int8_t flipping, float flipPhase,
              bool resetArmed = false);
// The tapped card grows from its slot to fill the glass; p 0..1.
void uiZoom(const Spread &s, uint8_t pos, float p);
// One card as large as the glass allows, then its meaning as centered text.
void uiCardBig(const Spread &s, uint8_t pos);
void uiCardFlip(const Spread &s, uint8_t pos, float phase);
void uiMeaning(const Spread &s, uint8_t pos);
// Lays out the composed text (mutated in place). Returns page count.
uint8_t uiInnerPrepare(char *text);
void uiInner(const Spread &s, uint8_t page, uint8_t pages);

bool uiDeckHit(int16_t x, int16_t y);
int8_t uiMenuHit(int16_t x, int16_t y);
int8_t uiSettingsHit(int16_t x, int16_t y);
int8_t uiSlotHit(const Spread &s, int16_t x, int16_t y);
