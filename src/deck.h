// Deck registry.
//
// A deck owns what stays the same in every language: its artwork, its card
// values and numerals, its elemental attributions, and which pairs of cards
// carry a relationship. Everything a reader actually reads is language data
// and arrives in a pack (src/strings.h), so a deck is a set of pictures and a
// set of numbers, not a set of sentences.
#pragma once

#include <stdint.h>

#include "glyphs.h"
#include "strings.h"
#include "tarot_data.h"

// The current product ships the Major Arcana only. The IDs are stable across
// decks so the reading engine can keep semantic relationships stable while a
// deck changes its names, numerals, artwork or attributions.
static const uint8_t MAJOR_COUNT = 22;

// Which two cards relate, and whether the order matters. The prose that says
// what the relationship means is pair p of the deck's pack.
struct DeckPair {
  uint8_t a, b;
  bool ordered;
};

// The reading engine stays combinatorial, but its prose is supplied by the
// deck's language pack. Keeping these templates out of the engine lets an
// artwork pack carry a different interpretive voice without creating a lookup
// table for every possible three-card spread.
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

// Each member points into the loaded pack. The run lengths are fixed and are
// mirrored by deck_strings() in scripts/build_strings.py.
struct DeckReadingStyle {
  const char *const *rowNames;             // four Fool's Journey bands
  const char *const *rowGlosses;           // meaning of each band
  const char *const *arcTemplates;         // DeckArcTemplate
  const char *const *planeTemplates;       // DeckPlaneTemplate
  const char *const *dominantTemplates;    // DeckDominantTemplate
  const char *const *transitionTemplates;  // DeckTransitionTemplate
  const char *hiddenTemplate;              // sum, hidden card, essence
  const char *hiddenRepeatedTemplate;      // hidden card also on the table
};

struct DeckDefinition {
  const char *id;          // filesystem-safe identifier, e.g. "waite-smith"
  const char *assetDir;    // optional LittleFS directory below /decks/
  const char *backAsset;   // optional back basename; nullptr uses procedural
  const Glyph *glyphs;     // nullptr when a deck has no glyph set
  const uint8_t *values;   // card value by stable card identity
  const char *const *numerals;   // Roman numerals: the same in every language
  const Element *elements;
  // A deck whose card names are its own identity rather than a translation.
  // Marseille is French everywhere, the way a painting keeps its title. When
  // set, these names are used and the pack's names are ignored.
  const char *const *nativeNames;
  const DeckPair *pairs;
  uint8_t pairCount;
  uint8_t cardCount;
};

extern const DeckDefinition DECKS[];
extern const uint8_t DECK_COUNT;

// A deck's text in one language: the pack, plus the card table built over it.
// Everything else points straight into the pack, so this is one allocation
// for the file, one for its string table, and this struct.
struct DeckText {
  const DeckDefinition *deck;
  StringPack pack;
  CardInfo cards[MAJOR_COUNT];
  DeckReadingStyle style;
  const char *const *pairText;   // pairCount entries, parallel to deck->pairs
  const char *name;              // the deck's display name in this language
};

// Loads <root>/lang/<code>/<deck id>.pack. Returns false and leaves the struct
// empty if the file is missing or malformed; the caller falls back. `root` is
// empty on the device and the build directory in the host tools.
bool deckTextLoad(DeckText &t, const DeckDefinition &deck, const char *lang,
                  const char *root = "");
void deckTextFree(DeckText &t);

// The selected deck's text in the selected language, and the UI strings that
// go with it. Both change together: a language is only half applied if the
// menu switches and the cards do not.
extern DeckText deckText;

// Loads the UI pack and the deck pack for `lang`, falling back to English when
// either is missing so a bad or removed pack cannot leave the device with no
// words at all. Returns false when the fallback was used.
bool langApply(const char *lang, uint8_t deckId);

// The language codes present on the filesystem, sorted, written into `out`.
// Returns how many were found. The firmware never holds a list of languages
// of its own: a language exists because its directory does.
static const uint8_t LANG_CODE_MAX = 6;
uint8_t langList(char out[][LANG_CODE_MAX], uint8_t max);

// The same list, read once and kept. langList() opens the filesystem, and the
// settings screen redraws several times a second; the set of directories can
// only change on an uploadfs, which is a reboot.
uint8_t langCount();
const char *langCodeAt(uint8_t i);
uint8_t langIndexOf(const char *code);

const DeckDefinition &deckById(uint8_t id);
static inline const CardInfo &deckCard(const DeckText &t, uint8_t id) {
  return t.cards[id < t.deck->cardCount ? id : 0];
}
uint8_t deckValue(const DeckDefinition &deck, uint8_t id);
uint8_t deckCardForValue(const DeckDefinition &deck, uint8_t value);
