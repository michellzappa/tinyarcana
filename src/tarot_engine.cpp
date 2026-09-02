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

// ---- Curated pairs: a before b (ordered), or either order (unordered) ----
struct Pair {
  uint8_t a, b;
  bool ordered;
  const char *text;
};

static const Pair PAIRS[] = {
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

uint8_t tarotHiddenCard(const Reading &r) {
  uint16_t sum = (uint16_t)(r.card[0] + r.card[1] + r.card[2]);
  while (sum > 21) {
    uint16_t s = 0;
    for (uint16_t v = sum; v; v /= 10) s = (uint16_t)(s + v % 10);
    sum = s;
  }
  return (uint8_t)sum;
}

static bool opposed(Element a, Element b) {
  return (a == EL_FIRE && b == EL_WATER) || (a == EL_WATER && b == EL_FIRE) ||
         (a == EL_AIR && b == EL_EARTH) || (a == EL_EARTH && b == EL_AIR);
}

size_t tarotCompose(const Reading &r, char *buf, size_t n) {
  size_t len = 0;
  buf[0] = 0;
  const uint8_t a = r.card[0], b = r.card[1], c = r.card[2];
  const CardInfo &A = CARDS[a];
  const CardInfo &B = CARDS[b];
  const CardInfo &C = CARDS[c];

  // ---- The arc ----
  app(buf, n, len, "#THE ARC\n");
  if (a < b && b < c) {
    appf(buf, n, len, "The numbers climb from %s to %s. This story moves deeper into the journey, not back to its start. Each card is further along than the one before it.\n", A.numeral, C.numeral);
  } else if (a > b && b > c) {
    appf(buf, n, len, "The numbers fall from %s to %s. The sequence runs backward: what lies ahead is an earlier lesson, revisited with what you now know.\n", A.numeral, C.numeral);
  } else if (b > a && b > c) {
    appf(buf, n, len, "%s in the present is the summit. Past and future both sit lower. Whatever this is, you are in the thick of it now, and the road down is shorter than the climb was.\n", B.name);
  } else {
    appf(buf, n, len, "%s in the present is the low point between two higher cards. The dip is the passage, not the destination.\n", B.name);
  }

  const uint8_t ra = rowOf(a), rb = rowOf(b), rc = rowOf(c);
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
    const CardInfo &P = CARDS[r.card[i]];
    const CardInfo &Q = CARDS[r.card[i + 1]];
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
  for (const Pair &p : PAIRS) {
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
  const uint16_t sum = (uint16_t)(a + b + c);
  const uint8_t h = tarotHiddenCard(r);
  const CardInfo &H = CARDS[h];
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
