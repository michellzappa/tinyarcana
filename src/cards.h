// Card bitmaps from LittleFS (data/<env>/decks/<id>/NN_168x295.565), cached
// in PSRAM, blitted with rounded corners and scaled for both UI sizes. A deck
// may also provide <back>_168x295.565; decks without one use the procedural
// back below.
#pragma once

#include <Arduino.h>

#include "deck.h"

// S: the three-card spread. L: one card, as large as the glass allows.
enum CardSize : uint8_t { CARD_S = 0, CARD_L = 1 };

// Drawing sizes. The source bitmap is one smaller shared size so five decks
// can coexist in flash; cards.cpp samples it for both views.
// L is the largest 0.569 rectangle inside the 233 px circle, less a margin.
static const int16_t CARD_W[2] = {120, 224};
static const int16_t CARD_H[2] = {211, 394};

static const int16_t CARD_SRC_W = 168;
static const int16_t CARD_SRC_H = 295;

bool cardsBegin();
bool cardsReady();
bool cardsSelectDeck(const DeckDefinition &deck);
const uint16_t *cardBitmap(uint8_t idx, CardSize sz);
void cardPreload(uint8_t idx);

// Face: width scaled by squash (0..1) about cx; top edge at y.
void cardDrawFace(uint8_t idx, CardSize sz, int16_t cx, int16_t y, float squash);
// Face resampled from the large bitmap to any w x h (zoom animation).
void cardDrawFaceScaled(uint8_t idx, int16_t cx, int16_t y, int16_t w, int16_t h);
// Deck back at any size. Uses the active deck's bitmap when available and
// otherwise draws the procedural RWS back; glow 0..1 lifts the procedural gold.
void cardDrawBack(int16_t cx, int16_t y, int16_t w, int16_t h, float squash,
                  float glow);
