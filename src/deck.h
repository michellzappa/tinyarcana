// Deck registry. A reading stores card identities inside the selected deck;
// presentation and card values come from this descriptor rather than from a
// single global CARDS table.
#pragma once

#include <Arduino.h>

#include "glyphs.h"
#include "tarot_data.h"

// The current product ships the Major Arcana only. The IDs are stable across
// decks so the reading engine can keep semantic relationships stable while a
// deck changes its names, numerals, artwork or attributions.
static const uint8_t MAJOR_COUNT = 22;

struct DeckPair {
  uint8_t a, b;
  bool ordered;
  const char *text;
};

struct DeckDefinition {
  const char *id;          // filesystem-safe identifier, e.g. "rws"
  const char *name;        // user-facing name
  const char *assetDir;    // LittleFS directory below /decks/
  const CardInfo *cards;
  const Glyph *glyphs;     // nullptr when a deck has no glyph set
  const uint8_t *values;   // card value by stable card identity
  const DeckPair *pairs;   // deck-specific relationship prose
  uint8_t pairCount;
  uint8_t cardCount;
};

extern const DeckDefinition DECKS[];
extern const uint8_t DECK_COUNT;

const DeckDefinition &deckById(uint8_t id);
const CardInfo &deckCard(const DeckDefinition &deck, uint8_t id);
uint8_t deckValue(const DeckDefinition &deck, uint8_t id);
uint8_t deckCardForValue(const DeckDefinition &deck, uint8_t value);
