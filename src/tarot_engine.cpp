#include "tarot_engine.h"

#include <stdio.h>
#include <string.h>

#include "strings.h"
#include "tarot_data.h"

// ---- Fool's Journey rows ----
static uint8_t rowOf(uint8_t n) { return n == 0 ? 0 : (uint8_t)((n - 1) / 7 + 1); }

// ---- Helpers ----
static void app(char *buf, size_t n, size_t &len, const char *s) {
  const size_t l = strlen(s);
  if (len + l + 1 >= n) return;
  memcpy(buf + len, s, l);
  len += l;
  buf[len] = 0;
}

// Templates are language data, so slot order belongs to the translator; the
// substitution itself lives in strings.cpp because the UI needs it too.
static void appf(char *buf, size_t n, size_t &len, const char *fmt,
                 const char *a = "", const char *b = "", const char *c = "",
                 const char *d = "") {
  char tmp[400];
  strFormat(tmp, sizeof tmp, fmt, a, b, c, d);
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

size_t tarotCompose(const DeckText &t, const StringPack &ui, const Reading &r,
                    char *buf, size_t n) {
  size_t len = 0;
  buf[0] = 0;
  const DeckDefinition &deck = *t.deck;
  const uint8_t a = r.card[0], b = r.card[1], c = r.card[2];
  const uint8_t av = deckValue(deck, a), bv = deckValue(deck, b),
               cv = deckValue(deck, c);
  const CardInfo &A = deckCard(t, a);
  const CardInfo &B = deckCard(t, b);
  const CardInfo &C = deckCard(t, c);
  const DeckReadingStyle &style = t.style;
  const char *const POSITION_NAME[3] = {packGet(ui, ENG_POS_PAST),
                                        packGet(ui, ENG_POS_PRESENT),
                                        packGet(ui, ENG_POS_FUTURE)};
  const char *const ELEMENT_NAME[4] = {packGet(ui, ENG_EL_FIRE),
                                       packGet(ui, ENG_EL_WATER),
                                       packGet(ui, ENG_EL_AIR),
                                       packGet(ui, ENG_EL_EARTH)};

  // ---- The arc ----
  app(buf, n, len, packGet(ui, ENG_HEAD_ARC));
  if (av < bv && bv < cv) {
    appf(buf, n, len, style.arcTemplates[DECK_ARC_CLIMB], A.numeral, C.numeral);
  } else if (av > bv && bv > cv) {
    appf(buf, n, len, style.arcTemplates[DECK_ARC_FALL], A.numeral, C.numeral);
  } else if (bv > av && bv > cv) {
    appf(buf, n, len, style.arcTemplates[DECK_ARC_SUMMIT], B.name);
  } else {
    appf(buf, n, len, style.arcTemplates[DECK_ARC_DIP], B.name);
  }

  const uint8_t ra = rowOf(av), rb = rowOf(bv), rc = rowOf(cv);
  if (ra == rb && rb == rc) {
    appf(buf, n, len, style.planeTemplates[DECK_PLANE_SAME], style.rowNames[ra], style.rowGlosses[ra]);
  } else if (rc > ra) {
    appf(buf, n, len, style.planeTemplates[DECK_PLANE_RISE], style.rowNames[ra], style.rowNames[rc], style.rowGlosses[ra], style.rowGlosses[rc]);
  } else if (rc < ra) {
    appf(buf, n, len, style.planeTemplates[DECK_PLANE_DESCEND], style.rowNames[ra], style.rowNames[rc]);
  } else {
    appf(buf, n, len, style.planeTemplates[DECK_PLANE_DETOUR], style.rowNames[ra], style.rowNames[rb]);
  }

  // ---- Elements ----
  app(buf, n, len, packGet(ui, ENG_HEAD_ELEMENTS));
  uint8_t count[4] = {0, 0, 0, 0};
  count[A.element]++; count[B.element]++; count[C.element]++;
  int8_t dom = -1;
  for (uint8_t e = 0; e < 4; e++) if (count[e] >= 2) dom = (int8_t)e;
  app(buf, n, len,
      style.dominantTemplates[dom < 0 ? DECK_DOM_BALANCED : (uint8_t)dom]);

  for (uint8_t i = 0; i < 2; i++) {
    const CardInfo &P = deckCard(t, r.card[i]);
    const CardInfo &Q = deckCard(t, r.card[i + 1]);
    const char *pn = POSITION_NAME[i];
    const char *qn = POSITION_NAME[i + 1];
    if (P.element == Q.element) {
      appf(buf, n, len, style.transitionTemplates[DECK_TRANSITION_SAME], pn,
           qn, ELEMENT_NAME[P.element]);
    } else if (opposed(P.element, Q.element)) {
      if (P.element == EL_FIRE || P.element == EL_WATER)
        appf(buf, n, len,
             style.transitionTemplates[DECK_TRANSITION_FIRE_WATER], pn,
             ELEMENT_NAME[P.element], qn, ELEMENT_NAME[Q.element]);
      else
        appf(buf, n, len,
             style.transitionTemplates[DECK_TRANSITION_AIR_EARTH], pn,
             ELEMENT_NAME[P.element], qn, ELEMENT_NAME[Q.element]);
    } else {
      appf(buf, n, len, style.transitionTemplates[DECK_TRANSITION_FRIENDLY], pn,
           ELEMENT_NAME[P.element], qn, ELEMENT_NAME[Q.element]);
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
    else if (p.ordered && a == p.a && c == p.b) { hit = true; where = packGet(ui, ENG_ACROSS); }
    else if (!p.ordered) {
      if ((a == p.b && b == p.a) || (b == p.b && c == p.a)) hit = true;
      else if ((a == p.a && c == p.b) || (a == p.b && c == p.a)) { hit = true; where = packGet(ui, ENG_ACROSS); }
    }
    if (!hit) continue;
    if (!anyPair) app(buf, n, len, packGet(ui, ENG_HEAD_THREADS));
    anyPair = true;
    appf(buf, n, len, "%1%2\n", t.pairText[pi], where);
  }

  // ---- The hidden card ----
  app(buf, n, len, packGet(ui, ENG_HEAD_HIDDEN));
  const uint16_t sum = (uint16_t)(deckValue(deck, a) + deckValue(deck, b) +
                                  deckValue(deck, c));
  const uint8_t h = tarotHiddenCard(deck, r);
  const CardInfo &H = deckCard(t, h);
  // The sum reads as one number until it has to be reduced, and the sentence
  // that says so is language data like everything else.
  char nums[64], one[8], two[8];
  snprintf(one, sizeof one, "%u", sum);
  if (sum > 21) {
    snprintf(two, sizeof two, "%u", h);
    strFormat(nums, sizeof nums, packGet(ui, ENG_SUM_REDUCED), one, two);
  } else {
    snprintf(nums, sizeof nums, "%s", one);
  }
  appf(buf, n, len, style.hiddenTemplate, nums, H.name, H.essence);
  for (uint8_t i = 0; i < 3; i++) {
    if (r.card[i] == h) {
      appf(buf, n, len, style.hiddenRepeatedTemplate, POSITION_NAME[i]);
    }
  }

  // ---- Closing ----
  app(buf, n, len, "\n");
  appf(buf, n, len, ">%1\n", C.question);
  return len;
}
