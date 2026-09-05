#include "deck.h"

#include "deck_content.h"

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

static const char *const RWS_ROW_NAME[4] = {
    "the threshold", "the outer world", "the inner world", "the greater world"};
static const char *const RWS_ROW_GLOSS[4] = {
    "the step before any of it",
    "roles, people and the things you can touch",
    "the tests the soul sets itself",
    "forces larger than any one life"};
static const char *const RWS_ARC[4] = {
    "The numbers climb from %s to %s. This story moves deeper into the journey, not back to its start. Each card is further along than the one before it.\n",
    "The numbers fall from %s to %s. The sequence runs backward: what lies ahead is an earlier lesson, revisited with what you now know.\n",
    "%s in the present is the summit. Past and future both sit lower. Whatever this is, you are in the thick of it now, and the road down is shorter than the climb was.\n",
    "%s in the present is the low point between two higher cards. The dip is the passage, not the destination.\n",
};
static const char *const RWS_PLANE[4] = {
    "All three cards live in %s: %s. The whole reading happens on one plane, so the answer is on that plane too.\n",
    "The reading rises from %s into %s. What began as %s ends as %s.\n",
    "The reading descends from %s to %s. Something abstract is coming down to earth, where it can finally be handled.\n",
    "Past and future share %s; only the present steps into %s. The detour is the point.\n",
};
static const char *const RWS_DOMINANT[5] = {
    "Fire dominates. This reading runs on will and momentum. Watch what it burns through on the way.\n",
    "Water dominates. Feeling and intuition carry this; facts come second to what you sense.\n",
    "Air dominates. This is about thought, choice and what gets said. Ideas are the terrain.\n",
    "Earth dominates. This is practical, slow and real: bodies, money, ground. Nothing here is abstract.\n",
    "Three different elements, none in charge. The reading is balanced and will not tip until you lean on it.\n",
};
static const char *const RWS_TRANSITION[4] = {
    "%s and %s share %s, so the movement between them is smooth. Nothing resists the change.\n",
    "%s (%s) and %s (%s) quench each other: drive meets feeling, and neither wins outright.\n",
    "%s (%s) and %s (%s) sit on opposed elements: ideas against ground. One has to yield to the other.\n",
    "%s (%s) feeds %s (%s). The elements are friendly, so this transition costs less than it looks.\n",
};
static const DeckReadingStyle RWS_READING_STYLE = {
    RWS_ROW_NAME, RWS_ROW_GLOSS, RWS_ARC, RWS_PLANE, RWS_DOMINANT,
    RWS_TRANSITION,
    "The three numbers add to %s. That names %s as the card beneath the spread. %s\n",
    "It is already on the table, in the %s position, which doubles its weight.\n",
};

// GPTarot keeps the same major-card identities for now, but its first reading
// pass is image-led and more intuitive than the RWS reading. Its card table
// and curated pair prose can be split out independently in the next content
// pass without changing the engine again.
static const char *const GPTAROT_ROW_NAME[4] = {
    "the first image", "the visible world", "the turning inward", "the larger pattern"};
static const char *const GPTAROT_ROW_GLOSS[4] = {
    "the impulse before a choice",
    "what is happening around you",
    "what this asks you to feel and understand",
    "the pattern only distance reveals"};
static const char *const GPTAROT_ARC[4] = {
    "The sequence brightens from %s to %s. Follow the images forward: each one opens the next door.\n",
    "The sequence turns back from %s to %s. An older image is returning, but you meet it with different eyes.\n",
    "%s is the image everything currently gathers around. Past and future frame it; the present is where the signal is strongest.\n",
    "%s is the quiet image between two louder ones. Do not mistake the pause for an ending.\n",
};
static const char *const GPTAROT_PLANE[4] = {
    "All three images belong to %s: %s. Stay with what this layer is showing you.\n",
    "The reading moves from %s into %s. What starts as %s becomes %s.\n",
    "The reading comes down from %s to %s. A feeling or symbol is asking to become something you can do.\n",
    "Past and future share %s; the present moves through %s. The contrast is part of the message.\n",
};
static const char *const GPTAROT_DOMINANT[5] = {
    "Fire is loud here. Let desire show you where the energy wants to go, then give it a shape.\n",
    "Water is loud here. Notice the feeling before you explain it away; the image is speaking through it.\n",
    "Air is loud here. A thought, word or choice changes the picture. Name it plainly.\n",
    "Earth is loud here. The message wants a body: a boundary, a task, a place or a next step.\n",
    "No element takes the lead. Let the images speak together instead of forcing one answer too soon.\n",
};
static const char *const GPTAROT_TRANSITION[4] = {
    "%s and %s share %s, so the story flows naturally between them. Notice what repeats.\n",
    "%s (%s) and %s (%s) pull in different directions: instinct meets feeling. The tension is information.\n",
    "%s (%s) and %s (%s) pull in different directions: thought meets the tangible world. Find the bridge.\n",
    "%s (%s) feeds %s (%s). This is the easiest transition in the spread; use its momentum.\n",
};
static const DeckReadingStyle GPTAROT_READING_STYLE = {
    GPTAROT_ROW_NAME, GPTAROT_ROW_GLOSS, GPTAROT_ARC, GPTAROT_PLANE,
    GPTAROT_DOMINANT, GPTAROT_TRANSITION,
    "The images add to %s, revealing %s beneath the spread. Its deeper signal is: %s\n",
    "That image is already present in the %s position, so its message is doubled.\n",
};

const DeckDefinition DECKS[] = {
    {"rws", "Rider-Waite-Smith", "rws", nullptr, CARDS, RWS_GLYPHS, RWS_VALUES,
     RWS_PAIRS, sizeof(RWS_PAIRS) / sizeof(RWS_PAIRS[0]), MAJOR_COUNT,
     &RWS_READING_STYLE},
    {"gptarot", "GPTarot", "gptarot", "back", GPTAROT_CARDS,
     GPTAROT_GLYPHS, GPTAROT_VALUES, GPTAROT_PAIRS, GPTAROT_PAIR_COUNT,
     MAJOR_COUNT, &GPTAROT_READING_STYLE},
    {"marseille", "Marseille", "marseille", nullptr, MARSEILLE_CARDS,
     nullptr, MARSEILLE_VALUES, MARSEILLE_PAIRS, MARSEILLE_PAIR_COUNT,
     MAJOR_COUNT, &MARSEILLE_READING_STYLE},
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
