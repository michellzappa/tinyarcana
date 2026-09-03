#include "ui.h"

#include <math.h>
#include <string.h>

#include "board_display.h"
#include "cards.h"
#include "deck.h"
#include "entropy.h"
#include "glyphs.h"
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
static const int16_t HINT_Y = 404;        // one-line hint baseline
static const int16_t DOTS_Y = 416;

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

// The meaning page body: a touch wider than the reading column so the longest
// meanings still hold five lines at 21 px.
static int16_t meaningWidthAt(int16_t baseline, int16_t *xLeft) {
#if UI_ROUND
  int16_t h = (int16_t)(chordHalf((int16_t)(baseline - 6)) - 44);
  if (h < 40) h = 40;
  *xLeft = (int16_t)(CX - h);
  return (int16_t)(2 * h);
#else
  return readWidthAt(baseline, xLeft);
#endif
}

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
static void hint(const char *a, const char *b = nullptr, int16_t y = HINT_Y) {
#if UI_ROUND
  if (b) {
    // The chord narrows towards the bottom of the glass, so the wider string
    // takes the upper line. At the old 12 px and HINT_Y 444 the longest of
    // these ran 219 px across a 172 px chord and the bezel ate the ends.
    if (txtWidth(lora_label, a, -1, 1) < txtWidth(lora_label, b, -1, 1)) {
      const char *t = a;
      a = b;
      b = t;
    }
    txtCenter(lora_label, a, CX, (int16_t)(y - 24), COL_GOLD_DIM, 1);
    txtCenter(lora_label, b, CX, y, COL_GOLD_DIM, 1);
  } else {
    txtCenter(lora_label, a, CX, y, COL_GOLD_DIM, 1);
  }
#else
  char buf[96];
  if (b) snprintf(buf, sizeof buf, "%s   %s", a, b);
  txtCenter(lora_label, b ? buf : a, CX, y, COL_GOLD_DIM, 1);
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

// A wedge of the rim, drawn as a fan of short radial lines.
//
// fillArc() is the obvious call and the wrong one here: Arduino_GFX scans the
// arc's bounding box, and for an arc at rim radius that box is the whole
// screen. Twenty-one of them cost 190 ms a frame, which is most of why the
// deck screen ran at 4 fps. This touches only the segment's own pixels, and
// because it is trigonometry rather than an arc call it wraps past 360
// degrees on its own with no seam to split.
static void ringSeg(int16_t outer, int16_t inner, float a0, float a1, uint16_t col) {
  static const float STEP = 0.25f;   // ~1 px of arc at rim radius
  for (float ang = a0; ang <= a1; ang += STEP) {
    const float rad = ang * 0.01745329f;
    const float c = cosf(rad), sn = sinf(rad);
    gfx->drawLine((int16_t)(CX + inner * c), (int16_t)(CY + inner * sn),
                  (int16_t)(CX + outer * c), (int16_t)(CY + outer * sn), col);
  }
}

// Twenty-one ticks around the rim, one per numbered trump, counted out
// clockwise from twelve o'clock: the deck assembling itself before it can be
// dealt. The Fool is unnumbered and stands outside the sequence, so he is the
// dot already at the centre rather than a mark on the wheel. Same inscribed
// circle rimFill() uses, pulled in clear of the bezel.
static void bootTicks(float t) {
  static const uint8_t N = 21;
  static const float FIRST = 0.12f;
  static const float SPAN = 0.62f;
  static const float HALF = 1.15f;    // half-width of each tick, degrees
  // The boot screen runs at about 7 frames per second, so a per-tick fade is
  // invisible: each frame advances roughly three ticks. They snap on instead.
  const int16_t r = (int16_t)((SCR_W < SCR_H ? SCR_W : SCR_H) / 2 - 8);
  const int16_t inner = (int16_t)(r - 12);
  for (uint8_t i = 0; i < N; i++) {
    if (t < FIRST + SPAN * ((float)i / (float)N)) break;
    const float mid = 270.0f + 360.0f * ((float)i / (float)N);
    ringSeg(r, inner, mid - HALF, mid + HALF, COL_GOLD);
  }
}

// The wordmark rises, holds, then dissolves before the deck appears.
static float bootWordAlpha(float t) {
  static const float IN0 = 0.30f, IN1 = 0.55f, OUT0 = 0.78f, OUT1 = 1.04f;
  if (t <= IN0 || t >= OUT1) return 0.0f;
  if (t < IN1) return easeOut((t - IN0) / (IN1 - IN0));
  if (t < OUT0) return 1.0f;
  return 1.0f - easeOut((t - OUT0) / (OUT1 - OUT0));
}

void uiBoot(uint32_t ageMs, bool fsOk, bool touchOk) {
  gfx->clear(COL_BG);
  const float t = ageMs / 2600.0f;
  const float e = easeOut(t);
#if UI_ROUND
  const int16_t cy = CY - 34, titleY = CY + 112;
#else
  const int16_t cy = 196, titleY = 332;
#endif
  const int16_t r = (int16_t)(8 + 60 * e);
  const uint16_t col = blend565(COL_BG, COL_GOLD, (uint8_t)(255 * (t < 1 ? t : 1)));
  gfx->drawCircle(CX, cy, r, col);
  gfx->drawCircle(CX, cy, (int16_t)(r * 0.66f), blend565(COL_BG, COL_GOLD_DIM, (uint8_t)(255 * e)));
  gfx->drawCircle(CX, cy, (int16_t)(r * 0.36f), blend565(COL_BG, COL_GOLD_DIM, (uint8_t)(170 * e)));
  gfx->fillCircle(CX, cy, 2, col);
  bootTicks(t);
  const float wa = bootWordAlpha(t);
  if (wa > 0.0f) {
    const uint8_t a = (uint8_t)(255 * wa);
    // One word, two weights: the size is dim, the deck is bright. Drawn as
    // two pieces so the pair still centres as a single wordmark.
    const int16_t wTiny = txtWidth(lora_name, "tiny");
    const int16_t wArc = txtWidth(lora_name, "arcana");
    const int16_t x0 = (int16_t)(CX - (wTiny + wArc) / 2);
    txtDraw(lora_name, "tiny", x0, titleY, blend565(COL_BG, COL_GOLD_DIM, a));
    txtDraw(lora_name, "arcana", (int16_t)(x0 + wTiny), titleY,
            blend565(COL_BG, COL_IVORY, a));
  }
  if (!fsOk) hint("CARD IMAGES MISSING", "pio run -t uploadfs");
  else if (!touchOk) hint("TOUCH NOT FOUND");
}

// ---------------- Deck ----------------
static void deckTitle() {
#if UI_ROUND
  txtCenter(lora_title, "Tarot", CX, 80, COL_IVORY);
#else
  txtCenter(lora_title, "Tarot", CX, 70, COL_IVORY);
#endif
}

// The shuffle charge fills the screen's own rim: a gold arc growing
// clockwise from twelve o'clock along the edge of the glass. On the square
// board the same arc runs inside the largest circle that fits.
static void rimFill(float progress) {
  static const uint8_t N = 21;      // one per numbered trump, as on the boot screen
  static const float GAP = 2.6f;    // degrees of unlit rim between segments
  const int16_t r = (SCR_W < SCR_H ? SCR_W : SCR_H) / 2 - 2;
  const int16_t thick = 5;
  const float step = 360.0f / (float)N;
  const float p = progress < 0.0f ? 0.0f : (progress > 1.0f ? 1.0f : progress);

  // Lit straight off progress, the same way bootTicks() reads off its own
  // clock. An earlier version advanced at most one segment per frame, which
  // sounds like it should look more deliberate and does the opposite: the ring
  // falls behind the charge all the way up and then catches up in one jump at
  // the end. If a frame covers two segments, two appear, and it still reads as
  // counting.
  const uint8_t lit = (uint8_t)(p * (float)N);

  // Unlike the boot screen, which counts the trumps in order, the shuffle
  // fills at random: it is a shuffle, so the ring should not read as counting.
  // The order is drawn once per hold from the same hardware entropy the deal
  // uses, and stays fixed for that hold so segments never flicker between
  // frames. entropyDraw with n == range is a permutation.
  static uint8_t order[N];
  static float lastP = 2.0f;
  if (p < lastP) entropyDraw(order, N, N);
  lastP = p;

  // The unlit track is one thin ring, not twenty-one arcs.
  gfx->drawArc(CX, CY, r, (int16_t)(r - thick), 0, 360, COL_RULE);
  for (uint8_t i = 0; i < lit; i++) {
    const float a0 = 270.0f + step * (float)order[i] + GAP * 0.5f;
    const float a1 = 270.0f + step * (float)(order[i] + 1) - GAP * 0.5f;
    ringSeg(r, (int16_t)(r - thick), a0, a1, COL_GOLD);
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

// ---------------- Menu ----------------
void uiMenu(uint8_t selected) {
  gfx->clear(COL_BG);
  txtCenter(lora_head, "Menu", CX, HEAD_Y, COL_IVORY);
  rule(HEAD_Y + 14, COL_GOLD_DIM);
  const char *items[] = {"HOW TO READ", "SETTINGS"};
  for (uint8_t i = 0; i < 2; i++) {
    const uint16_t col = i == selected ? COL_GOLD : COL_IVORY;
    txtCenter(lora_label, items[i], CX, (int16_t)(190 + i * 58), col, 3);
  }
  hint("TAP OUTSIDE: BACK", "BOOT: NEXT   PWR: OPEN");
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

// ---------------- Settings ----------------
void uiSettings(const AppSettings &settings, uint8_t selected) {
  gfx->clear(COL_BG);
  txtCenter(lora_head, "Settings", CX, HEAD_Y, COL_IVORY);
  rule(HEAD_Y + 14, COL_GOLD_DIM);

  const char *labels[] = {"DECK", "BRIGHTNESS", "SOUND", "BENEATH THE SPREAD"};
  char value[48];
  for (uint8_t i = 0; i < 4; i++) {
    const int16_t y = (int16_t)(145 + i * 58);
    const uint16_t col = i == selected ? COL_GOLD : COL_DIM;
    txtCenter(lora_small, labels[i], CX, y, col, 2);
    if (i == 0) {
      snprintf(value, sizeof value, "%s", deckById(settings.deckId).name);
    } else if (i == 1) {
      snprintf(value, sizeof value, "%u%%",
               (unsigned int)((settings.brightness * 100u + 127u) / 255u));
    } else if (i == 2) {
      if (settings.volume)
        snprintf(value, sizeof value, "%u%%", settings.volume);
      else
        snprintf(value, sizeof value, "OFF");
    } else {
      snprintf(value, sizeof value, "%s", settings.showHiddenCard ? "ON" : "OFF");
    }
    txtCenter(lora_label, value, CX, (int16_t)(y + 23),
              i == selected ? COL_IVORY : COL_DIM, 2);
  }
  hint("TAP BOTTOM: BACK", "BOOT: NEXT   PWR: CHANGE");
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

static void spreadBase(const Spread &s, int8_t flipping, float flipPhase, int8_t hide);

void uiSpread(const Spread &s, int8_t flipping, float flipPhase) {
  spreadBase(s, flipping, flipPhase, -1);
}

void uiZoom(const Spread &s, uint8_t pos, float p) {
  spreadBase(s, -1, 0, (int8_t)pos);
  const float u = easeOut(p);
  const int16_t bigY = (int16_t)((SCR_H - CARD_H[CARD_L]) / 2);
  const int16_t cx = (int16_t)(SLOT_CX[pos] + (CX - SLOT_CX[pos]) * u);
  const int16_t y = (int16_t)(SLOT_Y + (bigY - SLOT_Y) * u);
  const int16_t w = (int16_t)(CARD_W[CARD_S] + (CARD_W[CARD_L] - CARD_W[CARD_S]) * u);
  const int16_t h = (int16_t)(CARD_H[CARD_S] + (CARD_H[CARD_L] - CARD_H[CARD_S]) * u);
  cardDrawFaceScaled(s.reading.card[pos], cx, y, w, h);
}

static void spreadBase(const Spread &s, int8_t flipping, float flipPhase, int8_t hide) {
  gfx->clear(COL_BG);
  uint8_t n = 0;
  for (uint8_t i = 0; i < 3; i++) if (s.revealed[i]) n++;
  txtCenter(lora_head, n == 3 ? "Your reading" : "Turn the cards", CX, HEAD_Y, COL_IVORY);
  rule(HEAD_Y + 14, COL_RULE);

  const int16_t cardW = CARD_W[CARD_S], cardH = CARD_H[CARD_S];
  for (uint8_t i = 0; i < 3; i++) {
    drawSlotLabel(i, s.revealed[i] ? COL_GOLD : COL_GOLD_DIM);
    const uint8_t idx = s.reading.card[i];
    if ((int8_t)i == hide) {
      continue;
    } else if ((int8_t)i == flipping) {
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
      const DeckDefinition &deck = deckById(s.deck);
      txtWrapped(lora_small, deckCard(deck, idx).name,
                 (int16_t)(SLOT_CX[i] - cardW / 2 - 2),
                 (int16_t)(SLOT_Y + cardH + 21), (int16_t)(cardW + 4), 16, COL_IVORY, 2, true);
    }
  }

  if (n == 3 && appSettings.showHiddenCard) {
    const DeckDefinition &deck = deckById(s.deck);
    const uint8_t h = tarotHiddenCard(deck, s.reading);
    char buf[64];
    snprintf(buf, sizeof buf, "Beneath them: %s", deckCard(deck, h).name);
#if UI_ROUND
    // 226 px at its longest ("Beneath them: The High Priestess"), so it needs
    // a chord of at least that: 384 gives 318.
    txtCenter(lora_italic, buf, CX, 384, COL_DIM);
    hint("TAP A CARD TO READ IT", nullptr, 416);
#else
    txtCenter(lora_italic, buf, CX, 380, COL_DIM);
    hint("TAP A CARD TO READ IT");
#endif
  } else if (n == 3) {
    hint("TAP A CARD TO READ IT", "PWR: INNER   BOOT+PWR: CLOSE");
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
  const DeckDefinition &deck = deckById(s.deck);
  const CardInfo &c = deckCard(deck, idx);
  char label[24];
  upper(label, sizeof label, POSITION_NAME[pos]);
#if UI_ROUND
  const int16_t labelY = 78, nameY = 122, keysY = 154, ruleY = 170, bodyY = 204;
#else
  const int16_t labelY = 50, nameY = 94, keysY = 126, ruleY = 142, bodyY = 176;
#endif
  const int16_t lineH = 30;
  txtCenter(lora_label, label, CX, labelY, COL_GOLD, 3);
  txtCenter(lora_name, c.name, CX, nameY, COL_IVORY);
  txtCenter(lora_keys, c.keywords, CX, keysY, COL_DIM);
  rule(ruleY, COL_GOLD_DIM);
  const int16_t after = txtWrappedFn(lora_meaning, cardPositionText(c, pos), meaningWidthAt, bodyY, lineH, COL_IVORY, 6);

  // The card's Golden Dawn glyph, with numeral, attribution and element,
  // hung a fixed distance under the last line so the page reads as one block.
  // scripts/preview_read.py BUMPED2 mirrors these numbers.
  int16_t glyphY = (int16_t)(after - lineH + 50);
  const int16_t glyphMaxY = DOTS_Y - 56;
  if (glyphY > glyphMaxY) glyphY = glyphMaxY;
  if (deck.glyphs) glyphDraw(deck.glyphs[idx], CX, glyphY, 46, COL_GOLD);
  char cap[48];
  char ruler[16], el[12];
  upper(ruler, sizeof ruler, c.ruler);
  upper(el, sizeof el, ELEMENT_NAME[c.element]);
  if (strcmp(ruler, el) == 0) snprintf(cap, sizeof cap, "%s   %s", c.numeral, el);
  else snprintf(cap, sizeof cap, "%s   %s   %s", c.numeral, ruler, el);
  txtCenter(lora_label, cap, CX, (int16_t)(glyphY + 44), COL_GOLD_DIM, 2);

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
  const DeckDefinition &deck = deckById(s.deck);
#if UI_ROUND
  const int16_t headY = 56, subY = 76, ruleY = 88;
#else
  const int16_t headY = 46, subY = 66, ruleY = 76;
#endif
  txtCenter(lora_head, "The inner reading", CX, headY, COL_IVORY);
  char sub[96];
  snprintf(sub, sizeof sub, "%s  /  %s  /  %s",
           deckCard(deck, s.reading.card[0]).name,
           deckCard(deck, s.reading.card[1]).name,
           deckCard(deck, s.reading.card[2]).name);
  int16_t x0 = 0;
  if (txtWidth(lora_small, sub) > widthAt(subY, &x0))
    snprintf(sub, sizeof sub, "%s / %s / %s",
             deckCard(deck, s.reading.card[0]).numeral,
             deckCard(deck, s.reading.card[1]).numeral,
             deckCard(deck, s.reading.card[2]).numeral);
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

  // No hint here: the reading fills the page down to INNER_BOTTOM and any
  // hint line would sit on top of it. The dots carry the paging, and the help
  // screen documents the controls.
  dots(pages, page, DOTS_Y);
}

// ---------------- Hit testing ----------------
bool uiDeckHit(int16_t x, int16_t y) {
  return x >= DECK_CX - DECK_W / 2 - 20 && x <= DECK_CX + DECK_W / 2 + 20 &&
         y >= DECK_Y - 20 && y <= DECK_Y + DECK_H + 20;
}

int8_t uiMenuHit(int16_t x, int16_t y) {
  if (x < 50 || x > SCR_W - 50) return -1;
  if (y >= 155 && y < 220) return 0;
  if (y >= 220 && y < 285) return 1;
  return -1;
}

int8_t uiSettingsHit(int16_t x, int16_t y) {
  if (x < 30 || x > SCR_W - 30) return -1;
  for (int8_t i = 0; i < 4; i++) {
    const int16_t top = (int16_t)(112 + i * 58);
    if (y >= top && y < top + 58) return i;
  }
  return -1;
}

int8_t uiSlotHit(int16_t x, int16_t y) {
  if (y < SLOT_Y - 24 || y > SLOT_Y + CARD_H[CARD_S] + 40) return -1;
  for (int8_t i = 0; i < 3; i++) {
    if (x >= SLOT_CX[i] - CARD_W[CARD_S] / 2 - 7 && x <= SLOT_CX[i] + CARD_W[CARD_S] / 2 + 7)
      return i;
  }
  return -1;
}
