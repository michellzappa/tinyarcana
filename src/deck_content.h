// Card meanings and deck-specific reading data.
#pragma once

#include "deck.h"

extern const CardInfo GPTAROT_CARDS[MAJOR_COUNT];
extern const uint8_t GPTAROT_VALUES[MAJOR_COUNT];
extern const Glyph GPTAROT_GLYPHS[MAJOR_COUNT];
extern const DeckPair GPTAROT_PAIRS[];
extern const uint8_t GPTAROT_PAIR_COUNT;

extern const CardInfo THOTH_CARDS[MAJOR_COUNT];
extern const uint8_t THOTH_VALUES[MAJOR_COUNT];
extern const Glyph THOTH_GLYPHS[MAJOR_COUNT];
extern const DeckPair THOTH_PAIRS[];
extern const uint8_t THOTH_PAIR_COUNT;
extern const DeckReadingStyle THOTH_READING_STYLE;

extern const CardInfo MARSEILLE_CARDS[MAJOR_COUNT];
extern const uint8_t MARSEILLE_VALUES[MAJOR_COUNT];
extern const DeckPair MARSEILLE_PAIRS[];
extern const uint8_t MARSEILLE_PAIR_COUNT;
extern const DeckReadingStyle MARSEILLE_READING_STYLE;
