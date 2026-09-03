#include "tarot_engine.h"

#include <stdio.h>
#include <string.h>

#include "tarot_data.h"

// ---- Fool's Journey rows ----
static uint8_t rowOf(uint8_t n) { return n == 0 ? 0 : (uint8_t)((n - 1) / 7 + 1); }
static const char *const ROW_NAME[4] = {
    "the threshold", "the outer world", "the inner world", "the greater world"};
static const char *const ROW_GLOSS[4] = {
    "the step before any of it",
    "roles, people and the things you can touch",
    "the tests the soul sets itself",
    "forces larger than any one life"};

// ---- Helpers ----
static void app(char *buf, size_t n, size_t &len, const char *s) {
  const size_t l = strlen(s);
  if (len + l + 1 >= n) return;
  memcpy(buf + len, s, l);
  len += l;
  buf[len] = 0;
}

static void appf(char *buf, size_t n, size_t &len, const char *fmt,
                 const char *a = "", const char *b = "", const char *c = "",
                 const char *d = "") {
  char tmp[400];
  snprintf(tmp, sizeof tmp, fmt, a, b, c, d);
  app(buf, n, len, tmp);
}

uint8_t tarotHiddenCard(const DeckDefinition &deck, const Reading &r) {
  uint16_t sum = (uint16_t)(deckValue(deck, r.card[0]) +
                            deckValue(deck, r.card[1]) +
                            deckValue(deck, r.card[2]));
  while (sum > 21) {
    uint16_t s = 0;
    for (uint16_t v = sum; v; v /= 10) s = (uint16_t)(s + v % 10);
    sum = s;
  }
  return deckCardForValue(deck, (uint8_t)sum);
}

static bool opposed(Element a, Element b) {
  return (a == EL_FIRE && b == EL_WATER) || (a == EL_WATER && b == EL_FIRE) ||
         (a == EL_AIR && b == EL_EARTH) || (a == EL_EARTH && b == EL_AIR);
}

size_t tarotCompose(const DeckDefinition &deck, const Reading &r, char *buf,
                    size_t n) {
  size_t len = 0;
  buf[0] = 0;
  const uint8_t a = r.card[0], b = r.card[1], c = r.card[2];
  const uint8_t av = deckValue(deck, a), bv = deckValue(deck, b),
               cv = deckValue(deck, c);
  const CardInfo &A = deckCard(deck, a);
  const CardInfo &B = deckCard(deck, b);
  const CardInfo &C = deckCard(deck, c);

  // ---- The arc ----
  app(buf, n, len, "#THE ARC\n");
  if (av < bv && bv < cv) {
    appf(buf, n, len, "The numbers climb from %s to %s. This story moves deeper into the journey, not back to its start. Each card is further along than the one before it.\n", A.numeral, C.numeral);
  } else if (av > bv && bv > cv) {
    appf(buf, n, len, "The numbers fall from %s to %s. The sequence runs backward: what lies ahead is an earlier lesson, revisited with what you now know.\n", A.numeral, C.numeral);
  } else if (bv > av && bv > cv) {
    appf(buf, n, len, "%s in the present is the summit. Past and future both sit lower. Whatever this is, you are in the thick of it now, and the road down is shorter than the climb was.\n", B.name);
  } else {
    appf(buf, n, len, "%s in the present is the low point between two higher cards. The dip is the passage, not the destination.\n", B.name);
  }

  const uint8_t ra = rowOf(av), rb = rowOf(bv), rc = rowOf(cv);
  if (ra == rb && rb == rc) {
    appf(buf, n, len, "All three cards live in %s: %s. The whole reading happens on one plane, so the answer is on that plane too.\n", ROW_NAME[ra], ROW_GLOSS[ra]);
  } else if (rc > ra) {
    appf(buf, n, len, "The reading rises from %s into %s. What began as %s ends as %s.\n", ROW_NAME[ra], ROW_NAME[rc], ROW_GLOSS[ra], ROW_GLOSS[rc]);
  } else if (rc < ra) {
    appf(buf, n, len, "The reading descends from %s to %s. Something abstract is coming down to earth, where it can finally be handled.\n", ROW_NAME[ra], ROW_NAME[rc]);
  } else {
    appf(buf, n, len, "Past and future share %s; only the present steps into %s. The detour is the point.\n", ROW_NAME[ra], ROW_NAME[rb]);
  }

  // ---- Elements ----
  app(buf, n, len, "\n#ELEMENTS\n");
  uint8_t count[4] = {0, 0, 0, 0};
  count[A.element]++; count[B.element]++; count[C.element]++;
  int8_t dom = -1;
  for (uint8_t e = 0; e < 4; e++) if (count[e] >= 2) dom = (int8_t)e;
  if (dom == EL_FIRE) app(buf, n, len, "Fire dominates. This reading runs on will and momentum. Watch what it burns through on the way.\n");
  else if (dom == EL_WATER) app(buf, n, len, "Water dominates. Feeling and intuition carry this; facts come second to what you sense.\n");
  else if (dom == EL_AIR) app(buf, n, len, "Air dominates. This is about thought, choice and what gets said. Ideas are the terrain.\n");
  else if (dom == EL_EARTH) app(buf, n, len, "Earth dominates. This is practical, slow and real: bodies, money, ground. Nothing here is abstract.\n");
  else app(buf, n, len, "Three different elements, none in charge. The reading is balanced and will not tip until you lean on it.\n");

  for (uint8_t i = 0; i < 2; i++) {
    const CardInfo &P = deckCard(deck, r.card[i]);
    const CardInfo &Q = deckCard(deck, r.card[i + 1]);
    const char *pn = POSITION_NAME[i];
    const char *qn = POSITION_NAME[i + 1];
    if (P.element == Q.element) {
      appf(buf, n, len, "%s and %s share %s, so the movement between them is smooth. Nothing resists the change.\n", pn, qn, ELEMENT_NAME[P.element]);
    } else if (opposed(P.element, Q.element)) {
      if (P.element == EL_FIRE || P.element == EL_WATER)
        appf(buf, n, len, "%s (%s) and %s (%s) quench each other: drive meets feeling, and neither wins outright.\n", pn, ELEMENT_NAME[P.element], qn, ELEMENT_NAME[Q.element]);
      else
        appf(buf, n, len, "%s (%s) and %s (%s) sit on opposed elements: ideas against ground. One has to yield to the other.\n", pn, ELEMENT_NAME[P.element], qn, ELEMENT_NAME[Q.element]);
    } else {
      appf(buf, n, len, "%s (%s) feeds %s (%s). The elements are friendly, so this transition costs less than it looks.\n", pn, ELEMENT_NAME[P.element], qn, ELEMENT_NAME[Q.element]);
    }
  }

  // ---- Threads ----
  bool anyPair = false;
  for (uint8_t pi = 0; pi < deck.pairCount; pi++) {
    const DeckPair &p = deck.pairs[pi];
    bool hit = false;
    const char *where = "";
    // Adjacent, in order.
    if ((a == p.a && b == p.b) || (b == p.a && c == p.b)) { hit = true; }
    else if (p.ordered && a == p.a && c == p.b) { hit = true; where = " (across the reading)"; }
    else if (!p.ordered) {
      if ((a == p.b && b == p.a) || (b == p.b && c == p.a)) hit = true;
      else if ((a == p.a && c == p.b) || (a == p.b && c == p.a)) { hit = true; where = " (across the reading)"; }
    }
    if (!hit) continue;
    if (!anyPair) app(buf, n, len, "\n#THREADS\n");
    anyPair = true;
    appf(buf, n, len, "%s%s\n", p.text, where);
  }

  // ---- The hidden card ----
  app(buf, n, len, "\n#THE HIDDEN CARD\n");
  const uint16_t sum = (uint16_t)(deckValue(deck, a) + deckValue(deck, b) +
                                  deckValue(deck, c));
  const uint8_t h = tarotHiddenCard(deck, r);
  const CardInfo &H = deckCard(deck, h);
  char nums[48];
  if (sum > 21) snprintf(nums, sizeof nums, "%u, which reduces to %u", sum, h);
  else snprintf(nums, sizeof nums, "%u", sum);
  appf(buf, n, len, "The three numbers add to %s. That names %s as the card beneath the spread. %s\n", nums, H.name, H.essence);
  for (uint8_t i = 0; i < 3; i++) {
    if (r.card[i] == h) {
      appf(buf, n, len, "It is already on the table, in the %s position, which doubles its weight.\n", POSITION_NAME[i]);
    }
  }

  // ---- Closing ----
  app(buf, n, len, "\n");
  appf(buf, n, len, ">%s\n", C.question);
  return len;
}
