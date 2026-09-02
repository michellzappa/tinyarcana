#include "ui.h"

#include <math.h>
#include <string.h>

#include "board_display.h"
#include "cards.h"
#include "tarot_data.h"
#include "text.h"

// ---------------- Geometry ----------------
static const int16_t CX = SCR_W / 2;
static const int16_t CY = SCR_H / 2;

#if UI_ROUND
// The glass is a circle of radius 233. Text lines take the chord at their
// row, inset by EDGE. Vertical anchors are chosen so nothing meets the arc.
static const int16_t R = 233;
static const int16_t EDGE = 22;
static const int16_t HEAD_Y = 60;         // screen header baseline
static const int16_t HINT_Y = 444;        // one-line hint baseline
static const int16_t HINT2_Y = 426;       // second hint line, above it
static const int16_t DOTS_Y = 408;

static int16_t chordHalf(int16_t y) {
  const int32_t dy = y - CY;
  const int32_t v = (int32_t)R * R - dy * dy;
  return v <= 0 ? 0 : (int16_t)sqrtf((float)v);
}

// Width available to a text line whose baseline is y (glyphs sit ~5px above).
static int16_t widthAt(int16_t baseline, int16_t *xLeft) {
  int16_t h = (int16_t)(chordHalf((int16_t)(baseline - 5)) - EDGE);
  if (h < 40) h = 40;
  *xLeft = (int16_t)(CX - h);
  return (int16_t)(2 * h);
}

// The readings sit in a narrower column: shorter lines read better and the
// block stays clear of the arc.
static const int16_t READ_EDGE = 52;
static int16_t readWidthAt(int16_t baseline, int16_t *xLeft) {
  int16_t h = (int16_t)(chordHalf((int16_t)(baseline - 6)) - READ_EDGE);
  if (h < 40) h = 40;
  *xLeft = (int16_t)(CX - h);
  return (int16_t)(2 * h);
}
#else
static const int16_t MARGIN = 14;
static const int16_t CONTENT_W = SCR_W - 2 * MARGIN;   // 340
static const int16_t HEAD_Y = 56;
static const int16_t HINT_Y = 434;
static const int16_t HINT2_Y = 434;
static const int16_t DOTS_Y = 412;

static int16_t widthAt(int16_t baseline, int16_t *xLeft) {
  (void)baseline;
  *xLeft = MARGIN;
  return CONTENT_W;
}

static const int16_t READ_MARGIN = 26;
static int16_t readWidthAt(int16_t baseline, int16_t *xLeft) {
  (void)baseline;
  *xLeft = READ_MARGIN;
  return (int16_t)(SCR_W - 2 * READ_MARGIN);
}
#endif

static const int16_t READ_LINE_H = 24;

static float easeOut(float t) {
  if (t < 0) t = 0;
  if (t > 1) t = 1;
  const float u = 1.0f - t;
  return 1.0f - u * u * u;
}

static void rule(int16_t y, uint16_t col) {
  gfx->drawFastHLine(CX - 70, y, 140, col);
}

// One hint line, or two on the round face where a long one would meet the arc.
static void hint(const char *a, const char *b = nullptr) {
#if UI_ROUND
  if (b) {
    txtCenter(lora_small, a, CX, HINT2_Y, COL_DIM, 1);
    txtCenter(lora_small, b, CX, HINT_Y, COL_DIM, 1);
  } else {
    txtCenter(lora_small, a, CX, HINT_Y, COL_DIM, 1);
  }
#else
  char buf[96];
  if (b) snprintf(buf, sizeof buf, "%s   %s", a, b);
  txtCenter(lora_small, b ? buf : a, CX, HINT_Y, COL_DIM);
#endif
}

static void dots(uint8_t n, uint8_t active, int16_t y) {
  for (uint8_t i = 0; i < n; i++) {
    const int16_t dx = (int16_t)(CX - (n - 1) * 6 + i * 12);
    if (i == active) gfx->fillCircle(dx, y, 3, COL_GOLD);
    else gfx->drawCircle(dx, y, 2, COL_GOLD_DIM);
  }
}

static void upper(char *dst, size_t n, const char *s) {
  size_t i = 0;
  for (; s[i] && i + 1 < n; i++) dst[i] = (char)toupper((unsigned char)s[i]);
  dst[i] = 0;
}

// ---------------- Boot ----------------
void uiBoot(uint32_t ageMs, bool fsOk, bool touchOk) {
  gfx->clear(COL_BG);
  const float t = ageMs / 1400.0f;
  const float e = easeOut(t);
#if UI_ROUND
  const int16_t cy = CY - 30, titleY = CY + 100, subY = CY + 128;
#else
  const int16_t cy = 200, titleY = 320, subY = 348;
#endif
  const int16_t r = (int16_t)(8 + 60 * e);
  const uint16_t col = blend565(COL_BG, COL_GOLD, (uint8_t)(255 * (t < 1 ? t : 1)));
  gfx->drawCircle(CX, cy, r, col);
  gfx->drawCircle(CX, cy, (int16_t)(r * 0.62f), blend565(COL_BG, COL_GOLD_DIM, (uint8_t)(255 * e)));
  gfx->fillCircle(CX, cy, 2, col);
  if (t > 0.35f) {
    const uint8_t a = (uint8_t)(255 * easeOut((t - 0.35f) / 0.5f));
    txtCenter(lora_title, "Tarot", CX, titleY, blend565(COL_BG, COL_IVORY, a));
    txtCenter(lora_small, "TWENTY-TWO MAJORS", CX, subY, blend565(COL_BG, COL_GOLD_DIM, a), 2);
  }
  if (!fsOk) hint("CARD IMAGES MISSING", "pio run -t uploadfs");
  else if (!touchOk) hint("TOUCH NOT FOUND");
}

// ---------------- Deck ----------------
static void deckTitle() {
#if UI_ROUND
  txtCenter(lora_title, "Tarot", CX, 72, COL_IVORY);
  txtCenter(lora_small, "PAST   PRESENT   FUTURE", CX, 96, COL_GOLD_DIM, 2);
#else
  txtCenter(lora_title, "Tarot", CX, 62, COL_IVORY);
  txtCenter(lora_small, "PAST   PRESENT   FUTURE", CX, 88, COL_GOLD_DIM, 2);
#endif
}

// The shuffle charge fills the screen's own rim: a gold arc growing
// clockwise from twelve o'clock along the edge of the glass. On the square
// board the same arc runs inside the largest circle that fits.
static void rimFill(float progress) {
  const int16_t r = (SCR_W < SCR_H ? SCR_W : SCR_H) / 2 - 2;
  const int16_t thick = 5;
  // Arduino_GFX arcs: 0 degrees is at three o'clock, increasing clockwise.
  gfx->drawArc(CX, CY, r, r - thick, 0, 360, COL_RULE);
  if (progress <= 0) return;
  const float sweep = 360.0f * (progress > 1 ? 1 : progress);
  const float start = 270.0f;
  const float end = start + sweep;
  if (end <= 360.0f) {
    gfx->fillArc(CX, CY, r, r - thick, start, end, COL_GOLD);
  } else {
    gfx->fillArc(CX, CY, r, r - thick, start, 360.0f, COL_GOLD);
    gfx->fillArc(CX, CY, r, r - thick, 0.0f, end - 360.0f, COL_GOLD);
  }
}

void uiDeck(uint32_t nowMs, bool holding, float progress) {
  gfx->clear(COL_BG);
  deckTitle();

  const float breath = 0.5f + 0.5f * sinf(nowMs / 1400.0f);
  float glow = holding ? 0.35f + 0.65f * progress : 0.15f + 0.25f * breath;
  int16_t jx = 0, jy = 0;
  if (holding) {
    const int16_t amp = (int16_t)(1 + 3 * progress);
    jx = (int16_t)((int32_t)(nowMs * 7) % (2 * amp + 1)) - amp;
    jy = (int16_t)((int32_t)(nowMs * 11) % (2 * amp + 1)) - amp;
  }
  // Stack: three backs offset to read as thickness.
  cardDrawBack(DECK_CX + 5, DECK_Y + 5, DECK_W, DECK_H, 1.0f, 0.0f);
  cardDrawBack(DECK_CX + 2, DECK_Y + 2, DECK_W, DECK_H, 1.0f, 0.05f);
  cardDrawBack(DECK_CX + jx, DECK_Y + jy, DECK_W, DECK_H, 1.0f, glow);

  if (holding) {
    rimFill(progress);
    hint(progress >= 1.0f ? "RELEASE TO CUT" : "SHUFFLING . . .");
  } else {
    hint("HOLD THE DECK TO SHUFFLE");
  }
}

void uiHelp() {
  gfx->clear(COL_BG);
  txtCenter(lora_head, "How to read", CX, HEAD_Y, COL_IVORY);
  rule(HEAD_Y + 14, COL_GOLD_DIM);
  const char *lines[] = {
      "Hold the deck to shuffle. Your touch feeds the draw, on top of the chip's hardware noise.",
      "Release to cut. Three cards are dealt: past, present, future.",
      "Tap a card to turn it. Tap it again to read it.",
      "BOOT steps through the cards. Press BOOT and PWR together, or hold BOOT, to close the reading.",
      "PWR opens the inner reading: how the three cards speak to each other.",
  };
  int16_t y = (int16_t)(HEAD_Y + 44);
  for (const char *l : lines) {
    y = txtWrappedFn(lora_body, l, widthAt, y, 20, COL_IVORY, 4);
    y = (int16_t)(y + 9);
  }
  hint("TAP TO GO BACK");
}

// ---------------- Cut and deal ----------------
void uiCut(float p) {
  gfx->clear(COL_BG);
  deckTitle();
  const float s = sinf(p * 3.14159f);
  const int16_t off = (int16_t)(58 * s);
  const int16_t lift = (int16_t)(10 * s);
  cardDrawBack(DECK_CX + 5, DECK_Y + 5, DECK_W, DECK_H, 1.0f, 0.0f);
  cardDrawBack(DECK_CX - off, DECK_Y + lift, DECK_W, DECK_H, 1.0f, 0.3f);
  cardDrawBack(DECK_CX + off, DECK_Y - lift, DECK_W, DECK_H, 1.0f, 0.6f);
  hint("CUT");
}

void uiDeal(float p) {
  gfx->clear(COL_BG);
  deckTitle();
  // No deck under the spread: the first card is the deck, and the moment
  // it moves the stack behind it is gone.
  for (uint8_t i = 0; i < 3; i++) {
    const float start = i * 0.22f;
    const float u = easeOut((p - start) / 0.5f);
    if (u <= 0) continue;
    const int16_t cx = (int16_t)(DECK_CX + (SLOT_CX[i] - DECK_CX) * u);
    const int16_t y = (int16_t)(DECK_Y + (SLOT_Y - DECK_Y) * u);
    const int16_t w = (int16_t)(DECK_W + (CARD_W[CARD_S] - DECK_W) * u);
    const int16_t h = (int16_t)(DECK_H + (CARD_H[CARD_S] - DECK_H) * u);
    cardDrawBack(cx, y, w, h, 1.0f, 0.5f * (1 - u) + 0.15f);
  }
}

void uiGather(float p) {
  gfx->clear(COL_BG);
  deckTitle();
  // Future returns first, then present, then past lands on top as the deck.
  for (int8_t i = 2; i >= 0; i--) {
    const float start = (2 - i) * 0.22f;
    const float u = easeOut((p - start) / 0.5f);
    const int16_t cx = (int16_t)(SLOT_CX[i] + (DECK_CX - SLOT_CX[i]) * u);
    const int16_t y = (int16_t)(SLOT_Y + (DECK_Y - SLOT_Y) * u);
    const int16_t w = (int16_t)(CARD_W[CARD_S] + (DECK_W - CARD_W[CARD_S]) * u);
    const int16_t h = (int16_t)(CARD_H[CARD_S] + (DECK_H - CARD_H[CARD_S]) * u);
    cardDrawBack(cx, y, w, h, 1.0f, 0.15f + 0.25f * u);
  }
}

// ---------------- Spread ----------------
static void drawSlotLabel(uint8_t i, uint16_t col) {
  char buf[12];
  upper(buf, sizeof buf, POSITION_NAME[i]);
  txtCenter(lora_small, buf, SLOT_CX[i], SLOT_Y - 12, col, 2);
}

void uiSpread(const Spread &s, int8_t flipping, float flipPhase) {
  gfx->clear(COL_BG);
  uint8_t n = 0;
  for (uint8_t i = 0; i < 3; i++) if (s.revealed[i]) n++;
  txtCenter(lora_head, n == 3 ? "Your reading" : "Turn the cards", CX, HEAD_Y, COL_IVORY);
  rule(HEAD_Y + 14, COL_RULE);

  const int16_t cardW = CARD_W[CARD_S], cardH = CARD_H[CARD_S];
  for (uint8_t i = 0; i < 3; i++) {
    drawSlotLabel(i, s.revealed[i] ? COL_GOLD : COL_GOLD_DIM);
    const uint8_t idx = s.reading.card[i];
    if ((int8_t)i == flipping) {
      if (flipPhase < 0.5f) {
        cardDrawBack(SLOT_CX[i], SLOT_Y, cardW, cardH, 1.0f - flipPhase * 2.0f, 0.6f);
      } else {
        cardDrawFace(idx, CARD_S, SLOT_CX[i], SLOT_Y, (flipPhase - 0.5f) * 2.0f);
      }
    } else if (s.revealed[i]) {
      cardDrawFace(idx, CARD_S, SLOT_CX[i], SLOT_Y, 1.0f);
    } else {
      cardDrawBack(SLOT_CX[i], SLOT_Y, cardW, cardH, 1.0f, 0.15f);
    }
    if (s.revealed[i] && (int8_t)i != flipping) {
      txtWrapped(lora_small, CARDS[idx].name, (int16_t)(SLOT_CX[i] - cardW / 2 - 2),
                 (int16_t)(SLOT_Y + cardH + 21), (int16_t)(cardW + 4), 16, COL_IVORY, 2, true);
    }
  }

  if (n == 3) {
    const uint8_t h = tarotHiddenCard(s.reading);
    char buf[64];
    snprintf(buf, sizeof buf, "Beneath them: %s", CARDS[h].name);
#if UI_ROUND
    txtCenter(lora_italic, buf, CX, 396, COL_DIM);
    hint("TAP A CARD TO READ IT", "PWR: INNER   BOOT+PWR: CLOSE");
#else
    txtCenter(lora_italic, buf, CX, 386, COL_DIM);
    hint("TAP A CARD TO READ IT", "PWR: INNER   BOOT+PWR: CLOSE");
#endif
  } else {
    hint("TAP A CARD TO TURN IT");
  }
}

// ---------------- One card, large ----------------
void uiCardBig(const Spread &s, uint8_t pos) {
  gfx->clear(COL_BG);
  const uint8_t idx = s.reading.card[pos];
  const int16_t y = (int16_t)((SCR_H - CARD_H[CARD_L]) / 2);
  cardDrawFace(idx, CARD_L, CX, y, 1.0f);
#if UI_ROUND
  char label[24];
  upper(label, sizeof label, POSITION_NAME[pos]);
  txtCenter(lora_small, label, CX, 26, COL_GOLD, 3);
  txtCenter(lora_small, "TAP TO READ", CX, HINT_Y, COL_DIM, 1);
#endif
}

// ---------------- Meaning ----------------
void uiMeaning(const Spread &s, uint8_t pos) {
  gfx->clear(COL_BG);
  const uint8_t idx = s.reading.card[pos];
  const CardInfo &c = CARDS[idx];
  char label[24];
  upper(label, sizeof label, POSITION_NAME[pos]);
#if UI_ROUND
  const int16_t labelY = 84, nameY = 118, keysY = 144, ruleY = 160, bodyY = 190, elY = 384;
#else
  const int16_t labelY = 56, nameY = 90, keysY = 116, ruleY = 132, bodyY = 162, elY = 384;
#endif
  txtCenter(lora_small, label, CX, labelY, COL_GOLD, 3);
  txtCenter(lora_title, c.name, CX, nameY, COL_IVORY);
  txtCenter(lora_italic, c.keywords, CX, keysY, COL_DIM);
  rule(ruleY, COL_GOLD_DIM);
  txtWrappedFn(lora_read, cardPositionText(idx, pos), readWidthAt, bodyY, READ_LINE_H, COL_IVORY, 7);

  char sub[32];
  char el[12];
  upper(el, sizeof el, ELEMENT_NAME[c.element]);
  snprintf(sub, sizeof sub, "%s   %s", c.numeral, el);
  txtCenter(lora_small, sub, CX, elY, COL_GOLD_DIM, 2);

  dots(3, pos, DOTS_Y);
  if (pos == 2) hint("TAP: SPREAD   BOOT: SPREAD", "PWR: INNER READING");
  else hint("TAP: SPREAD   BOOT: NEXT CARD", "PWR: INNER READING");
}

// ---------------- Inner reading ----------------
static const uint16_t INNER_MAX_LINES = 140;
static TxtLine innerLines[INNER_MAX_LINES];
static uint16_t innerCount = 0;
#if UI_ROUND
static const int16_t INNER_TOP = 110, INNER_BOTTOM = 388;
#else
static const int16_t INNER_TOP = 96, INNER_BOTTOM = 404;
#endif

uint8_t uiInnerPrepare(char *text) {
  uint8_t pages = 1;
  innerCount = txtLayout(text, readWidthAt, INNER_TOP, INNER_BOTTOM, innerLines,
                         INNER_MAX_LINES, &pages, lora_read, lora_read_italic, READ_LINE_H);
  return pages;
}

void uiInner(const Spread &s, uint8_t page, uint8_t pages) {
  gfx->clear(COL_BG);
#if UI_ROUND
  const int16_t headY = 56, subY = 76, ruleY = 88;
#else
  const int16_t headY = 46, subY = 66, ruleY = 76;
#endif
  txtCenter(lora_head, "The inner reading", CX, headY, COL_IVORY);
  char sub[96];
  snprintf(sub, sizeof sub, "%s  /  %s  /  %s", CARDS[s.reading.card[0]].name,
           CARDS[s.reading.card[1]].name, CARDS[s.reading.card[2]].name);
  int16_t x0 = 0;
  if (txtWidth(lora_small, sub) > widthAt(subY, &x0))
    snprintf(sub, sizeof sub, "%s / %s / %s", CARDS[s.reading.card[0]].numeral,
             CARDS[s.reading.card[1]].numeral, CARDS[s.reading.card[2]].numeral);
  txtCenter(lora_small, sub, CX, subY, COL_DIM);
  rule(ruleY, COL_GOLD_DIM);

  for (uint16_t i = 0; i < innerCount; i++) {
    const TxtLine &l = innerLines[i];
    if (l.page != page) continue;
    if (l.style == TXT_HEAD) {
      txtDraw(lora_small, l.s, l.x, l.y, COL_GOLD, (int16_t)l.len, 2);
    } else if (l.style == TXT_ITALIC) {
      txtDraw(lora_read_italic, l.s, l.x, l.y, COL_GOLD, (int16_t)l.len);
    } else {
      txtDraw(lora_read, l.s, l.x, l.y, COL_IVORY, (int16_t)l.len);
    }
  }

  dots(pages, page, DOTS_Y);
  if (page + 1 < pages) hint("TAP: NEXT PAGE", "PWR: BACK");
  else hint("TAP OR PWR: BACK");
}

// ---------------- Hit testing ----------------
bool uiDeckHit(int16_t x, int16_t y) {
  return x >= DECK_CX - DECK_W / 2 - 20 && x <= DECK_CX + DECK_W / 2 + 20 &&
         y >= DECK_Y - 20 && y <= DECK_Y + DECK_H + 20;
}

int8_t uiSlotHit(int16_t x, int16_t y) {
  if (y < SLOT_Y - 24 || y > SLOT_Y + CARD_H[CARD_S] + 40) return -1;
  for (int8_t i = 0; i < 3; i++) {
    if (x >= SLOT_CX[i] - CARD_W[CARD_S] / 2 - 7 && x <= SLOT_CX[i] + CARD_W[CARD_S] / 2 + 7)
      return i;
  }
  return -1;
}
