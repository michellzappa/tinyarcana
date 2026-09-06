#include "deck.h"

#include <stdio.h>

// Deck identity only. Every sentence a reader sees now lives in a language
// pack; what stays here is what a translation cannot change: which picture,
// which number, which element, and which pairs of cards mean something to
// each other. deck_content.cpp held the prose for the added decks and is gone
// for the same reason.

// The Golden Dawn attributions the Rider-Waite-Smith deck was designed
// around. GPTarot keeps the same major identities, so it shares the table
// rather than repeating it.
static const Glyph GOLDEN_DAWN_GLYPHS[MAJOR_COUNT] = {
    G_AIR, G_MERCURY, G_MOON, G_VENUS, G_ARIES, G_TAURUS, G_GEMINI,
    G_CANCER, G_LEO, G_VIRGO, G_JUPITER, G_LIBRA, G_WATER, G_SCORPIO,
    G_SAGITTARIUS, G_CAPRICORN, G_MARS, G_AQUARIUS, G_PISCES, G_SUN,
    G_FIRE, G_SATURN,
};

// Identity numbering: card id is its value. Marseille differs, below.
static const uint8_t SEQUENTIAL_VALUES[MAJOR_COUNT] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
};

// Marseille numbers Justice VIII and Strength XI, the reverse of the
// Rider-Waite-Smith order. The card identities stay stable so the engine's
// relationships survive the swap; only the values move.
static const uint8_t MARSEILLE_VALUES[MAJOR_COUNT] = {
    0, 1, 2, 3, 4, 5, 6, 7, 11, 9, 10,
    8, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
};

// waite-smith
static const char *const ws_NUMERALS[MAJOR_COUNT] = {
    "0", "I", "II", "III", "IV", "V", "VI", "VII",
    "VIII", "IX", "X", "XI", "XII", "XIII", "XIV", "XV",
    "XVI", "XVII", "XVIII", "XIX", "XX", "XXI",
};
static const Element ws_ELEMENTS[MAJOR_COUNT] = {
    EL_AIR, EL_AIR, EL_WATER, EL_EARTH, EL_FIRE,
    EL_EARTH, EL_AIR, EL_WATER, EL_FIRE, EL_EARTH,
    EL_FIRE, EL_AIR, EL_WATER, EL_WATER, EL_FIRE,
    EL_EARTH, EL_FIRE, EL_AIR, EL_WATER, EL_FIRE,
    EL_FIRE, EL_EARTH,
};
static const char *const ws_NAMES[MAJOR_COUNT] = {
    "The Fool", "The Magician", "The High Priestess", "The Empress",
    "The Emperor", "The Hierophant", "The Lovers", "The Chariot",
    "Strength", "The Hermit", "Wheel of Fortune", "Justice",
    "The Hanged Man", "Death", "Temperance", "The Devil",
    "The Tower", "The Star", "The Moon", "The Sun",
    "Judgement", "The World",
};

static const DeckPair ws_PAIRS[] = {
    {16, 17, true},
    {17, 16, true},
    {13, 19, true},
    {15, 16, true},
    {6, 15, true},
    {15, 6, true},
    {18, 19, true},
    {19, 18, true},
    {9, 5, false},
    {1, 2, false},
    {3, 4, false},
    {10, 11, false},
    {12, 13, true},
    {20, 21, true},
    {0, 21, false},
    {7, 8, true},
    {8, 7, true},
    {13, 16, false},
    {2, 18, false},
    {4, 16, false},
    {14, 15, false},
    {0, 13, false},
};

// gptarot
static const char *const gptarot_NUMERALS[MAJOR_COUNT] = {
    "0", "I", "II", "III", "IV", "V", "VI", "VII",
    "VIII", "IX", "X", "XI", "XII", "XIII", "XIV", "XV",
    "XVI", "XVII", "XVIII", "XIX", "XX", "XXI",
};
static const Element gptarot_ELEMENTS[MAJOR_COUNT] = {
    EL_AIR, EL_AIR, EL_WATER, EL_EARTH, EL_FIRE,
    EL_EARTH, EL_AIR, EL_WATER, EL_FIRE, EL_EARTH,
    EL_FIRE, EL_AIR, EL_WATER, EL_WATER, EL_FIRE,
    EL_EARTH, EL_FIRE, EL_AIR, EL_WATER, EL_FIRE,
    EL_FIRE, EL_EARTH,
};
static const char *const gptarot_NAMES[MAJOR_COUNT] = {
    "The Fool", "The Magician", "The High Priestess", "The Empress",
    "The Emperor", "The Hierophant", "The Lovers", "The Chariot",
    "Strength", "The Hermit", "Wheel of Fortune", "Justice",
    "The Hanged Man", "Death", "Temperance", "The Devil",
    "The Tower", "The Star", "The Moon", "The Sun",
    "Judgement", "The World",
};

static const DeckPair gptarot_PAIRS[] = {
    {0, 21, false},
    {1, 2, false},
    {3, 4, false},
    {5, 9, false},
    {6, 7, true},
    {7, 8, true},
    {8, 15, false},
    {10, 16, true},
    {11, 12, true},
    {12, 13, true},
    {13, 17, true},
    {14, 15, false},
    {16, 17, true},
    {17, 18, true},
    {18, 19, true},
    {19, 20, true},
    {20, 21, true},
    {2, 18, false},
    {4, 16, false},
    {6, 15, false},
    {9, 10, true},
    {14, 21, false},
};

// marseille
static const char *const marseille_NUMERALS[MAJOR_COUNT] = {
    "0", "I", "II", "III", "IV", "V", "VI", "VII",
    "XI", "IX", "X", "VIII", "XII", "XIII", "XIV", "XV",
    "XVI", "XVII", "XVIII", "XIX", "XX", "XXI",
};
static const Element marseille_ELEMENTS[MAJOR_COUNT] = {
    EL_AIR, EL_AIR, EL_WATER, EL_EARTH, EL_FIRE,
    EL_EARTH, EL_AIR, EL_WATER, EL_FIRE, EL_EARTH,
    EL_FIRE, EL_AIR, EL_WATER, EL_WATER, EL_FIRE,
    EL_EARTH, EL_FIRE, EL_AIR, EL_WATER, EL_FIRE,
    EL_FIRE, EL_EARTH,
};
static const char *const marseille_NAMES[MAJOR_COUNT] = {
    "Le Mat", "Le Bateleur", "La Papesse", "L'Imperatrice",
    "L'Empereur", "Le Pape", "L'Amoureux", "Le Chariot",
    "La Force", "L'Hermite", "La Roue de Fortune", "La Justice",
    "Le Pendu", "Arcane sans nom", "Temperance", "Le Diable",
    "La Maison-Dieu", "L'Etoile", "La Lune", "Le Soleil",
    "Le Jugement", "Le Monde",
};

static const DeckPair marseille_PAIRS[] = {
    {0, 21, false},
    {1, 2, false},
    {3, 4, false},
    {4, 5, false},
    {5, 9, false},
    {6, 7, true},
    {7, 8, true},
    {8, 15, false},
    {10, 16, true},
    {11, 12, true},
    {12, 13, true},
    {13, 17, true},
    {14, 15, false},
    {16, 17, true},
    {17, 18, true},
    {18, 19, true},
    {19, 20, true},
    {20, 21, true},
    {2, 18, false},
    {6, 15, false},
    {9, 10, true},
    {14, 21, false},
};

const DeckDefinition DECKS[] = {
    {"waite-smith", "waite-smith", nullptr, GOLDEN_DAWN_GLYPHS, SEQUENTIAL_VALUES,
     ws_NUMERALS, ws_ELEMENTS, nullptr,
     ws_PAIRS, sizeof(ws_PAIRS) / sizeof(ws_PAIRS[0]), MAJOR_COUNT},
    {"gptarot", "gptarot", "back", GOLDEN_DAWN_GLYPHS, SEQUENTIAL_VALUES,
     gptarot_NUMERALS, gptarot_ELEMENTS, nullptr,
     gptarot_PAIRS, sizeof(gptarot_PAIRS) / sizeof(gptarot_PAIRS[0]), MAJOR_COUNT},
    // Marseille keeps its French names in every language: the titles are the
    // deck, not a translation of the Rider-Waite-Smith ones.
    {"marseille", "marseille", nullptr, nullptr, MARSEILLE_VALUES,
     marseille_NUMERALS, marseille_ELEMENTS, marseille_NAMES,
     marseille_PAIRS, sizeof(marseille_PAIRS) / sizeof(marseille_PAIRS[0]), MAJOR_COUNT},
};

const uint8_t DECK_COUNT = sizeof(DECKS) / sizeof(DECKS[0]);

const DeckDefinition &deckById(uint8_t id) {
  return DECKS[id < DECK_COUNT ? id : 0];
}

uint8_t deckValue(const DeckDefinition &deck, uint8_t id) {
  return deck.values && id < deck.cardCount ? deck.values[id] : id;
}

uint8_t deckCardForValue(const DeckDefinition &deck, uint8_t value) {
  for (uint8_t i = 0; i < deck.cardCount; i++)
    if (deckValue(deck, i) == value) return i;
  return 0;
}

// ---- The deck's text, in one language ----
//
// Pack order is set by deck_strings() in scripts/build_strings.py: the deck
// name, then every card field, then the pair prose, then the reading-style
// runs. Only the card table is materialised; everything else points straight
// at the pack's own string table.
static const uint16_t STYLE_ROW_NAMES = 0;
static const uint16_t STYLE_ROW_GLOSSES = 4;
static const uint16_t STYLE_ARC = 8;
static const uint16_t STYLE_PLANE = 12;
static const uint16_t STYLE_DOMINANT = 16;
static const uint16_t STYLE_TRANSITION = 21;
static const uint16_t STYLE_HIDDEN = 25;
static const uint16_t STYLE_HIDDEN_REPEATED = 26;
static const uint16_t STYLE_RUN = 27;

bool deckTextLoad(DeckText &t, const DeckDefinition &deck, const char *lang,
                  const char *root) {
  t.deck = &deck;
  char path[256];
  langPath(path, sizeof path, root, lang, deck.id);
  if (!packLoad(t.pack, path)) return false;

  const uint16_t pairBase = (uint16_t)(1 + deck.cardCount * DECK_CARD_FIELDS);
  const uint16_t styleBase = (uint16_t)(pairBase + deck.pairCount);
  // A pack built for a different deck would otherwise read plausible strings
  // out of the wrong slots, which is harder to notice than a failed load.
  if (t.pack.count != styleBase + STYLE_RUN) {
    packFree(t.pack);
    return false;
  }

  t.name = packGet(t.pack, 0);
  for (uint8_t i = 0; i < deck.cardCount; i++) {
    const uint16_t b = (uint16_t)(1 + i * DECK_CARD_FIELDS);
    CardInfo &c = t.cards[i];
    c.name = deck.nativeNames ? deck.nativeNames[i] : packGet(t.pack, b + DCF_NAME);
    c.numeral = deck.numerals[i];
    c.element = deck.elements[i];
    c.ruler = packGet(t.pack, b + DCF_RULER);
    c.keywords = packGet(t.pack, b + DCF_KEYWORDS);
    c.essence = packGet(t.pack, b + DCF_ESSENCE);
    c.past = packGet(t.pack, b + DCF_PAST);
    c.present = packGet(t.pack, b + DCF_PRESENT);
    c.future = packGet(t.pack, b + DCF_FUTURE);
    c.question = packGet(t.pack, b + DCF_QUESTION);
  }

  t.pairText = &t.pack.s[pairBase];
  t.style.rowNames = &t.pack.s[styleBase + STYLE_ROW_NAMES];
  t.style.rowGlosses = &t.pack.s[styleBase + STYLE_ROW_GLOSSES];
  t.style.arcTemplates = &t.pack.s[styleBase + STYLE_ARC];
  t.style.planeTemplates = &t.pack.s[styleBase + STYLE_PLANE];
  t.style.dominantTemplates = &t.pack.s[styleBase + STYLE_DOMINANT];
  t.style.transitionTemplates = &t.pack.s[styleBase + STYLE_TRANSITION];
  t.style.hiddenTemplate = packGet(t.pack, styleBase + STYLE_HIDDEN);
  t.style.hiddenRepeatedTemplate = packGet(t.pack, styleBase + STYLE_HIDDEN_REPEATED);
  return true;
}

#ifdef ARDUINO
#include <LittleFS.h>
#include <string.h>
#endif

StringPack uiStrings = {nullptr, nullptr, 0};
DeckText deckText = {};

static char langCache[8][LANG_CODE_MAX];
static int8_t langCached = -1;

static void langFill() {
  if (langCached >= 0) return;
  langCached = (int8_t)langList(langCache, 8);
}

uint8_t langCount() { langFill(); return (uint8_t)langCached; }

const char *langCodeAt(uint8_t i) {
  langFill();
  return i < (uint8_t)langCached ? langCache[i] : "en";
}

uint8_t langIndexOf(const char *code) {
  langFill();
  for (uint8_t i = 0; i < (uint8_t)langCached; i++)
    if (strcmp(langCache[i], code) == 0) return i;
  return 0;
}

uint8_t langList(char out[][LANG_CODE_MAX], uint8_t max) {
  uint8_t n = 0;
#ifdef ARDUINO
  File dir = LittleFS.open("/lang");
  if (!dir || !dir.isDirectory()) return 0;
  for (File e = dir.openNextFile(); e && n < max; e = dir.openNextFile()) {
    if (!e.isDirectory()) { e.close(); continue; }
    const char *name = e.name();
    // LittleFS reports either a bare name or a full path depending on the
    // core; take the last segment either way.
    const char *slash = strrchr(name, '/');
    if (slash) name = slash + 1;
    if (name[0] && strlen(name) < LANG_CODE_MAX) {
      snprintf(out[n], LANG_CODE_MAX, "%s", name);
      n++;
    }
    e.close();
  }
  dir.close();
  for (uint8_t i = 1; i < n; i++)   // few entries; insertion sort is the right size
    for (uint8_t j = i; j && strcmp(out[j - 1], out[j]) > 0; j--) {
      char tmp[LANG_CODE_MAX];
      memcpy(tmp, out[j - 1], LANG_CODE_MAX);
      memcpy(out[j - 1], out[j], LANG_CODE_MAX);
      memcpy(out[j], tmp, LANG_CODE_MAX);
    }
#else
  (void)out;
  (void)max;
#endif
  return n;
}

// English is the fallback because it is the language the packs are generated
// from: scripts/build_strings.py refuses to build any other language that
// does not match it string for string, so `en` is the one that always fits.
bool langApply(const char *lang, uint8_t deckId) {
  const DeckDefinition &deck = deckById(deckId);
  StringPack ui = {nullptr, nullptr, 0};
  DeckText text = {};
  char path[256];

  // The two packs fall back separately. A language is usually translated one
  // deck at a time, and a half-finished language should show the decks it has
  // rather than reverting the whole device to English.
  bool ok = true;
  langPath(path, sizeof path, "", lang, "ui");
  if (!packLoad(ui, path)) {
    ok = false;
    langPath(path, sizeof path, "", "en", "ui");
    if (!packLoad(ui, path)) return false;
  }
  if (!deckTextLoad(text, deck, lang)) {
    ok = false;
    if (!deckTextLoad(text, deck, "en")) {
      packFree(ui);
      return false;
    }
  }

  packFree(uiStrings);
  deckTextFree(deckText);
  uiStrings = ui;
  deckText = text;
  return ok;
}

void deckTextFree(DeckText &t) {
  packFree(t.pack);
  t.pairText = nullptr;
  t.name = "";
}
