// The shapes a card's text takes. The text itself is language data and lives
// in assets/lang/<code>/<deck>.json, compiled into the packs that
// deckTextLoad() reads; see src/strings.h.
//
// Elements and rulers follow the Golden Dawn attributions (Book T): twelve
// zodiac signs, seven planets, and the three mother-letter elements for the
// Fool, the Hanged Man and Judgement. glyphs.cpp draws the matching symbol.
// Rows are the three sevens of the Fool's Journey: I-VII the outer world,
// VIII-XIV the inner world, XV-XXI the greater world; the Fool stands on the
// threshold.
//
// Each position text is written to fit six lines of lora_body at 340 px.
#pragma once

#include <stdint.h>

enum Element : uint8_t { EL_FIRE = 0, EL_WATER, EL_AIR, EL_EARTH };

// Built at load time by deckTextLoad(): the strings point into the language
// pack, while numeral and element come from the deck, which owns them in
// every language.
struct CardInfo {
  const char *name;
  const char *numeral;
  Element element;
  const char *ruler;
  const char *keywords;
  const char *essence;    // one line: what the card is
  const char *past;       // meaning in the Past position
  const char *present;
  const char *future;
  const char *question;   // closing question when it is the Future card
};

static inline const char *cardPositionText(const CardInfo &c, uint8_t pos) {
  return pos == 0 ? c.past : pos == 1 ? c.present : c.future;
}
