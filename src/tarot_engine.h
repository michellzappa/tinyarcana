// Inner mode: composes a reading of the three cards as a whole.
//
// It is deterministic and combinatorial rather than a lookup: the arc of the
// numbers, the rows of the Fool's Journey they cross, the elements and how
// adjacent ones treat each other, curated card pairs, and the quintessence
// card (the reduced sum) all contribute sentences. 1540 spreads, no two read
// the same.
#pragma once

#include <Arduino.h>

#include "deck.h"

struct Reading {
  uint8_t card[3];   // Past, Present, Future
};

// Quintessence: sum of the three, digit-reduced until it names a major.
uint8_t tarotHiddenCard(const DeckDefinition &deck, const Reading &r);

// Writes paragraphs separated by '\n'. '#' heading, '>' italic. Returns length.
size_t tarotCompose(const DeckDefinition &deck, const Reading &r, char *buf,
                    size_t n);
