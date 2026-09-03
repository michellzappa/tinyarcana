#include "deck.h"

// RWS is the first and default deck. The values are explicit so decks with a
// different Strength/Justice numbering can provide their own table later.
static const uint8_t RWS_VALUES[MAJOR_COUNT] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
    11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
};

static const Glyph RWS_GLYPHS[MAJOR_COUNT] = {
    G_AIR, G_MERCURY, G_MOON, G_VENUS, G_ARIES, G_TAURUS, G_GEMINI,
    G_CANCER, G_LEO, G_VIRGO, G_JUPITER, G_LIBRA, G_WATER, G_SCORPIO,
    G_SAGITTARIUS, G_CAPRICORN, G_MARS, G_AQUARIUS, G_PISCES, G_SUN,
    G_FIRE, G_SATURN,
};

static const DeckPair RWS_PAIRS[] = {
    {16, 17, true, "The Tower before the Star is the classic order: collapse, then healing. It holds here. What fell was in the way of the water."},
    {17, 16, true, "Hope followed by collapse. What you trusted gets tested. Keep the water and let the walls go."},
    {13, 19, true, "An ending followed by light. The loss is the doorway, not the room."},
    {15, 16, true, "Bondage, then rupture. The chain breaks because it has to, not because you chose it. You can still choose what you do next."},
    {6, 15, true, "A union that tightens into a bond. Check what the choice has become since you made it."},
    {15, 6, true, "From bondage to real choice. The chain gives way to a decision made freely, which is harder and better."},
    {18, 19, true, "Confusion clearing into daylight. What frightened you will look smaller in the morning."},
    {19, 18, true, "Clarity fading into fog. Enjoy what is plain now; you will need the memory of it."},
    {9, 5, false, "The Hermit and the Hierophant both teach, one alone and one in company. You are choosing how you learn."},
    {1, 2, false, "The Magician and the High Priestess are the two hands of one knowledge: doing and knowing. Use both."},
    {3, 4, false, "The Empress and the Emperor: growth and structure. The reading asks for both, in proportion."},
    {10, 11, false, "The Wheel and Justice: chance and consequence. Not everything that happens is deserved, and not everything deserved happens."},
    {12, 13, true, "Surrender, then release. What you stopped fighting can now end properly."},
    {20, 21, true, "The call answered and the circle closed. A proper ending is available here. Take it."},
    {0, 21, false, "The Fool and the World: the first step and the last, in one reading. This is a whole cycle, seen at once."},
    {7, 8, true, "Force gives way to patience. The second victory is quieter than the first."},
    {8, 7, true, "Patience becomes momentum. What you tamed now pulls the cart."},
    {13, 16, false, "Two endings in one spread. Whatever is finishing is finishing completely. Do not negotiate with it."},
    {2, 18, false, "The High Priestess and the Moon share the water. Much here is felt rather than known. Let it stay that way a while."},
    {4, 16, false, "The Emperor and the Tower: a structure and its fall. Ask which walls were load-bearing."},
    {14, 15, false, "Temperance beside the Devil: the measured cup and the bottomless one. The reading is about appetite and its limit."},
    {0, 13, false, "The Fool and Death travel together: a beginning that needs an ending to make room for it."},
};

const DeckDefinition DECKS[] = {
    {"rws", "Rider-Waite-Smith", "rws", CARDS, RWS_GLYPHS, RWS_VALUES,
     RWS_PAIRS, sizeof(RWS_PAIRS) / sizeof(RWS_PAIRS[0]), MAJOR_COUNT},
};

const uint8_t DECK_COUNT = sizeof(DECKS) / sizeof(DECKS[0]);

const DeckDefinition &deckById(uint8_t id) {
  return DECKS[id < DECK_COUNT ? id : 0];
}

const CardInfo &deckCard(const DeckDefinition &deck, uint8_t id) {
  return deck.cards[id < deck.cardCount ? id : 0];
}

uint8_t deckValue(const DeckDefinition &deck, uint8_t id) {
  return deck.values && id < deck.cardCount ? deck.values[id] : id;
}

uint8_t deckCardForValue(const DeckDefinition &deck, uint8_t value) {
  for (uint8_t i = 0; i < deck.cardCount; i++)
    if (deckValue(deck, i) == value) return i;
  return 0;
}
