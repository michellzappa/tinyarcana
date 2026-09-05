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

// The reading engine stays combinatorial, but its prose is supplied by the
// deck. Keeping these templates in the deck descriptor lets an artwork pack
// carry a different interpretive voice without creating a lookup table for
// every possible three-card spread.
enum DeckArcTemplate : uint8_t {
  DECK_ARC_CLIMB = 0,
  DECK_ARC_FALL,
  DECK_ARC_SUMMIT,
  DECK_ARC_DIP,
};

enum DeckPlaneTemplate : uint8_t {
  DECK_PLANE_SAME = 0,
  DECK_PLANE_RISE,
  DECK_PLANE_DESCEND,
  DECK_PLANE_DETOUR,
};

enum DeckDominantTemplate : uint8_t {
  DECK_DOM_FIRE = 0,
  DECK_DOM_WATER,
  DECK_DOM_AIR,
  DECK_DOM_EARTH,
  DECK_DOM_BALANCED,
};

enum DeckTransitionTemplate : uint8_t {
  DECK_TRANSITION_SAME = 0,
  DECK_TRANSITION_FIRE_WATER,
  DECK_TRANSITION_AIR_EARTH,
  DECK_TRANSITION_FRIENDLY,
};

struct DeckReadingStyle {
  const char *const *rowNames;             // four Fool's Journey bands
  const char *const *rowGlosses;           // meaning of each band
  const char *const *arcTemplates;         // DeckArcTemplate, format strings
  const char *const *planeTemplates;       // DeckPlaneTemplate, format strings
  const char *const *dominantTemplates;    // DeckDominantTemplate, prose
  const char *const *transitionTemplates;  // DeckTransitionTemplate, formats
  const char *hiddenTemplate;              // sum, hidden card, essence
  const char *hiddenRepeatedTemplate;      // hidden card also on the table
};

struct DeckDefinition {
  const char *id;          // filesystem-safe identifier, e.g. "rws"
  const char *name;        // user-facing name
  const char *assetDir;    // optional LittleFS directory below /decks/
  const char *backAsset;   // optional back basename; nullptr uses procedural
  const CardInfo *cards;
  const Glyph *glyphs;     // nullptr when a deck has no glyph set
  const uint8_t *values;   // card value by stable card identity
  const DeckPair *pairs;   // deck-specific relationship prose
  uint8_t pairCount;
  uint8_t cardCount;
  const DeckReadingStyle *reading;
};

extern const DeckDefinition DECKS[];
extern const uint8_t DECK_COUNT;

const DeckDefinition &deckById(uint8_t id);
const CardInfo &deckCard(const DeckDefinition &deck, uint8_t id);
uint8_t deckValue(const DeckDefinition &deck, uint8_t id);
uint8_t deckCardForValue(const DeckDefinition &deck, uint8_t value);
