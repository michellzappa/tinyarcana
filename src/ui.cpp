#include "ui.h"

#include <math.h>
#include <string.h>

#include "board_display.h"
#include "cards.h"
#include "deck.h"
#include "entropy.h"
#include "glyphs.h"
#include "power.h"
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

// Settings controls are deliberately generous touch targets. Keep these
// bounds shared by the renderer and hit testing so the visible button is the
// thing the user can tap.
#if UI_ROUND
static const int16_t SETTINGS_BUTTON_X = 52;
static const int16_t SETTINGS_BUTTON_W = 362;
// Four rows between the header and the back button. Losing the fifth row bought
// the height back: 52 px rows at step 58 end at 334, clear of the back button
// at 368, and a 52 px target holds a slider knob as well as a fingertip.
static const int16_t SETTINGS_BUTTON_TOP = 108;
static const int16_t SETTINGS_BUTTON_H = 52;
static const int16_t SETTINGS_BUTTON_STEP = 58;
static const int16_t SETTINGS_BACK_X = 116;
static const int16_t SETTINGS_BACK_W = 234;
static const int16_t SETTINGS_BACK_TOP = 368;
static const int16_t SETTINGS_BACK_H = 50;
#else
static const int16_t SETTINGS_BUTTON_X = 24;
static const int16_t SETTINGS_BUTTON_W = SCR_W - 48;
static const int16_t SETTINGS_BUTTON_TOP = 112;
static const int16_t SETTINGS_BUTTON_H = 54;
static const int16_t SETTINGS_BUTTON_STEP = 62;
static const int16_t SETTINGS_BACK_X = 82;
static const int16_t SETTINGS_BACK_W = SCR_W - 164;
static const int16_t SETTINGS_BACK_TOP = 346;
static const int16_t SETTINGS_BACK_H = 46;
#endif

// The menu uses the same direct-manipulation pattern as Settings: the
// control's visible bounds are also its touch target.
#if UI_ROUND
static const int16_t MENU_BUTTON_X = 52;
static const int16_t MENU_BUTTON_W = 362;
static const int16_t MENU_BUTTON_TOP = 142;
static const int16_t MENU_BUTTON_H = 68;
static const int16_t MENU_BUTTON_STEP = 86;
#else
static const int16_t MENU_BUTTON_X = 24;
static const int16_t MENU_BUTTON_W = SCR_W - 48;
static const int16_t MENU_BUTTON_TOP = 142;
static const int16_t MENU_BUTTON_H = 58;
static const int16_t MENU_BUTTON_STEP = 72;
#endif

// Inside a settings row. The brightness track is also its touch target: the
// finger sets the level where it lands, so the same numbers must answer both
// the renderer and uiBrightnessAtX().
static const int16_t ROW_PAD = 20;
static const int16_t SLIDER_X = (int16_t)(SETTINGS_BUTTON_X + ROW_PAD);
static const int16_t SLIDER_W = (int16_t)(SETTINGS_BUTTON_W - ROW_PAD - 90);
// 90 px is the readout plus clearance: "100%" is 46 px in lora_label at
// tracking 2, and the knob reaches 8 px past the end of the track.
// Never all the way off. A dark panel is a setting; a black one reads as a
// dead device, and the only control that could undo it is invisible.
static const uint8_t BRIGHT_MIN = 32;

// The draw-mode pair on the deck screen. One pill either side of the deck,
// level with its middle: the deck stands between the two choices rather than
// under them, and the band above it stays empty for the shuffle rim.
//
// The deck's own touch area runs x 138 to 328, so the pills are inset far
// enough to leave a margin on both sides of it.
#if UI_ROUND
static const int16_t DRAW_SEG_W = 56;
static const int16_t DRAW_SEG_H = 76;
static const int16_t DRAW_SEG_INSET = 30;
static const int16_t DRAW_SEG_CY = 240;    // the deck's own centre line
static const int16_t DRAW_DOT_STEP = 22;   // dots stack down the pill
// The settings link: the same small pill, above the deck rather than beside
// it, so the three controls read as one family.
static const int16_t LINK_W = 56;
static const int16_t LINK_H = 56;
static const int16_t LINK_TOP = 38;
#else
static const int16_t DRAW_SEG_W = 52;
static const int16_t DRAW_SEG_H = 72;
static const int16_t DRAW_SEG_INSET = 14;
static const int16_t DRAW_SEG_CY = 240;
static const int16_t DRAW_DOT_STEP = 20;
static const int16_t LINK_W = 52;
static const int16_t LINK_H = 52;
static const int16_t LINK_TOP = 26;
#endif
static const int16_t DRAW_SEG_TOP = (int16_t)(DRAW_SEG_CY - DRAW_SEG_H / 2);
static const int16_t LINK_X = (int16_t)(SCR_W / 2 - LINK_W / 2);
// A drawn control this small needs a hand-sized target around it. The pad is
// invisible: the pill says where to aim, the pad forgives the aim. It stops at
// the top of the deck artwork so a finger on the cards still shuffles.
static const int16_t TOUCH_PAD = 16;

// The close-the-reading mark, below the spread. 384 carries the quintessence
// line and 404 the hint, so 428 is the first clear row; its ring reaches 439
// against a chord that still allows 86 px either side of centre.
#if UI_ROUND
static const int16_t RESET_CY = 428;
#else
static const int16_t RESET_CY = 452;
#endif
static inline int16_t drawSegX(uint8_t i) {
  return (int16_t)(i == 0 ? DRAW_SEG_INSET
                          : SCR_W - DRAW_SEG_INSET - DRAW_SEG_W);
}

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

// ---------------- Power badge ----------------
// A small gauge low on the face, on the deck screen only: the deck screen is
// where the device sits idle, and every other screen already fills that band.
// Geometry is the lofiair badge, in this face's gold rather than its cyan.
#if UI_ROUND
static const int16_t PWR_TOP = 430;      // top edge of the 12 px cell
#else
static const int16_t PWR_TOP = 452;
#endif
static const int16_t PWR_BASE = (int16_t)(PWR_TOP + 11);   // percentage baseline

static void chargeBolt(int16_t cx, int16_t cy, uint16_t col) {
  gfx->fillTriangle((int16_t)(cx + 1), (int16_t)(cy - 6), (int16_t)(cx - 4), cy,
                    (int16_t)(cx + 1), cy, col);
  gfx->fillTriangle((int16_t)(cx - 1), cy, (int16_t)(cx + 4), cy,
                    (int16_t)(cx - 1), (int16_t)(cy + 6), col);
}

static void powerBadge() {
  const PowerState &p = powerPoll();
  // No PMU, and no cell on USB power: say nothing. A "PWR ?" on this face
  // reads as an error the reader can do nothing about.
  if (!p.valid || (!p.battery && !p.vbus)) return;

  const bool low = p.battery && p.percent >= 0 && p.percent <= 20;
  const uint16_t col = low ? COL_GOLD : COL_GOLD_DIM;

  char label[8] = "";
  if (p.percent >= 0) snprintf(label, sizeof label, "%d%%", (int)p.percent);
  const int16_t glyphW = 24;   // cell plus its cap, or the plug
  const int16_t gap = 6;
  const int16_t textW = label[0] ? txtWidth(lora_small, label, -1, 1) : 0;
  const int16_t total = (int16_t)(glyphW + (textW ? gap + textW : 0));
  const int16_t x = (int16_t)(CX - total / 2);

  if (p.battery) {
    gfx->drawRoundRect(x, PWR_TOP, 21, 12, 2, col);
    gfx->fillRoundRect((int16_t)(x + 21), (int16_t)(PWR_TOP + 4), 3, 5, 1, col);
    if (p.percent > 0) {
      int16_t fill = (int16_t)((17 * p.percent + 50) / 100);
      if (fill < 1) fill = 1;
      if (fill > 17) fill = 17;
      gfx->fillRoundRect((int16_t)(x + 2), (int16_t)(PWR_TOP + 2), fill, 8, 1, col);
    }
    if (p.charging) {
      gfx->fillRect((int16_t)(x + 7), (int16_t)(PWR_TOP + 1), 7, 10, COL_BG);
      chargeBolt((int16_t)(x + 10), (int16_t)(PWR_TOP + 6), col);
    }
  } else {
    // USB with no cell sensed: a side-on plug. There is nothing to gauge.
    gfx->fillRect(x, (int16_t)(PWR_TOP + 2), 7, 2, col);
    gfx->fillRect(x, (int16_t)(PWR_TOP + 9), 7, 2, col);
    gfx->fillRoundRect((int16_t)(x + 7), PWR_TOP, 12, 13, 2, col);
    gfx->fillRect((int16_t)(x + 19), (int16_t)(PWR_TOP + 5), 5, 3, col);
  }
  if (label[0])
    txtDraw(lora_small, label, (int16_t)(x + glyphW + gap), PWR_BASE, col, -1, 1);
}

static void dots(uint8_t n, uint8_t active, int16_t y) {
  for (uint8_t i = 0; i < n; i++) {
    const int16_t dx = (int16_t)(CX - (n - 1) * 6 + i * 12);
    if (i == active) gfx->fillCircle(dx, y, 3, COL_GOLD);
    else gfx->drawCircle(dx, y, 2, COL_GOLD_DIM);
  }
}

// Uppercase, UTF-8 aware over the charset the fonts carry. toupper() alone
// leaves every byte above 0x7F untouched, which renders "CAFE" as "CAFe" with
// the accent still lowercase. The two-byte rules below cover Latin-1 and the
// three Latin Extended-A letters in EXTRA; every case here keeps its length,
// so the buffer arithmetic stays simple. Lowercase sharp s and y-diaeresis
// have no same-length uppercase and pass through unchanged.
static void upper(char *dst, size_t n, const char *s) {
  size_t i = 0, j = 0;
  while (s[i] && j + 1 < n) {
    const uint8_t c = (uint8_t)s[i];
    if (c < 0x80) {
      dst[j++] = (char)toupper(c);
      i++;
      continue;
    }
    const uint8_t d = (uint8_t)s[i + 1];
    if (!d || j + 2 >= n) break;
    uint8_t e = d;
    if (c == 0xC3 && d >= 0xA0 && d <= 0xBE && d != 0xB7) e = (uint8_t)(d - 0x20);
    else if (c == 0xC5 && (d == 0x93 || d == 0xA1 || d == 0xBE)) e = (uint8_t)(d - 1);
    dst[j++] = (char)c;
    dst[j++] = (char)e;
    i += 2;
  }
  dst[j] = 0;
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
  // Keep the boot emblem concentric with the round glass, with the wordmark
  // layered over the circles instead of sitting below them.
  const int16_t cy = CY, titleY = CY + 14;
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
  if (!fsOk) hint(T(UI_FS_MISSING), "pio run -t uploadfs");
  else if (!touchOk) hint(T(UI_TOUCH_MISSING));
}

// ---------------- Deck ----------------

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

// The two quiet marks: a 22 px ring with one dot. Settings puts the dot on the
// ring, which reads as a dial. Closing a reading puts it in the middle, which
// is where the three cards end up when they gather.
//
// Neither gets a plate. A plate is for a control that changes the reading; a
// way out of a screen should not carry the same weight as the deck beside it.
static const int16_t MARK_R = 11;

static void mark(int16_t cx, int16_t cy, bool centred, uint16_t col) {
  gfx->drawCircle(cx, cy, MARK_R, col);
  if (centred) {
    gfx->fillCircle(cx, cy, 3, col);
  } else {
    gfx->fillCircle((int16_t)(cx + 8), (int16_t)(cy - 8), 3, COL_BG);
    gfx->fillCircle((int16_t)(cx + 8), (int16_t)(cy - 8), 3, col);
  }
}

// The way into Settings.
static void settingsLink() {
  mark((int16_t)(LINK_X + LINK_W / 2), (int16_t)(LINK_TOP + LINK_H / 2), false,
       COL_GOLD_DIM);
}

// One card or three, on the screen where the difference is felt: three cards
// are shuffled and cut with a hold, one is picked with a touch. Hiding that
// two screens away in Settings put the mode far from the ritual it governs.
static void drawModeSegment(bool one) {
  for (uint8_t i = 0; i < 2; i++) {
    const bool active = (i == 0) == one;
    const int16_t x = drawSegX(i);
    gfx->fillRoundRect(x, DRAW_SEG_TOP, DRAW_SEG_W, DRAW_SEG_H, 12,
                       active ? blend565(COL_BG, COL_GOLD_DIM, 48)
                              : blend565(COL_BG, COL_RULE, 150));
    gfx->drawRoundRect(x, DRAW_SEG_TOP, DRAW_SEG_W, DRAW_SEG_H, 12,
                       active ? COL_GOLD : COL_RULE);
    // One dot or three, not one word or two. The pill says how many cards are
    // coming without naming them, so the control needs no translation and the
    // two halves stay the same size in every language.
    const int16_t cx = (int16_t)(x + DRAW_SEG_W / 2);
    const int16_t cy = DRAW_SEG_CY;
    const uint16_t mark = active ? COL_GOLD : COL_DIM;
    const uint8_t n = i == 0 ? 1 : 3;
    for (uint8_t d = 0; d < n; d++)
      gfx->fillCircle(cx, (int16_t)(cy + ((int16_t)d - (n - 1) / 2) * DRAW_DOT_STEP),
                      5, mark);
  }
}

void uiDeck(uint32_t nowMs, bool holding, float progress) {
  gfx->clear(COL_BG);

  const float breath = 0.5f + 0.5f * sinf(nowMs / 1400.0f);
  float glow = holding ? 0.35f + 0.65f * progress : 0.15f + 0.25f * breath;
  int16_t jx = 0, jy = 0, riffle = 0, riffleY = 0;
  if (holding) {
    const int16_t amp = (int16_t)(1 + 3 * progress);
    jx = (int16_t)((int32_t)(nowMs * 7) % (2 * amp + 1)) - amp;
    jy = (int16_t)((int32_t)(nowMs * 11) % (2 * amp + 1)) - amp;
    // The riffle. The upper two cards swing apart and back together while the
    // rim fills, so the deck is visibly shuffling during the hold instead of
    // only at the cut, and it works harder as the charge builds. The period
    // stays near a second on purpose: this screen draws at a few frames a
    // second, and anything faster samples badly and strobes.
    const float period = 940.0f - 280.0f * progress;
    const float phase = sinf((float)nowMs * 6.2831853f / period);
    riffle = (int16_t)((7.0f + 25.0f * progress) * phase);
    riffleY = (int16_t)((2.0f + 6.0f * progress) * phase);
  }
  // Stack: three backs offset to read as thickness, straddling DECK_CX so the
  // pile is centred rather than the top card. Offsetting them all one way put
  // the whole stack 2 px right of centre while the top card's own design sat
  // on the centre line, which reads as the card being off to the left.
  // The bottom one holds still so the pile never looks like it has left the
  // table.
  cardDrawBack(DECK_CX + 3, DECK_Y + 3, DECK_W, DECK_H, 1.0f, 0.0f);
  cardDrawBack((int16_t)(DECK_CX - riffle), (int16_t)(DECK_Y - riffleY),
               DECK_W, DECK_H, 1.0f, 0.05f);
  cardDrawBack((int16_t)(DECK_CX - 3 + jx + riffle), (int16_t)(DECK_Y - 3 + jy + riffleY),
               DECK_W, DECK_H, 1.0f, glow);

  // A single card is picked, not shuffled and cut. The framing changes with
  // the setting rather than the spread, because no reading exists yet.
  const bool one = appSettings.singleCard;
  if (one) {
    // One card is picked with a touch, so there is no charge to show.
    hint(T(UI_PICK_CARD));
  } else if (holding) {
    rimFill(progress);
    hint(progress >= 1.0f ? T(UI_RELEASE_TO_CUT) : T(UI_SHUFFLING));
  } else {
    hint(T(UI_HOLD_TO_SHUFFLE));
  }
  // Not while shuffling: the hold is a commitment and the mode is already made.
  if (!holding) {
    drawModeSegment(one);
    settingsLink();
  }
  powerBadge();
}

// ---------------- Menu ----------------
void uiMenu(uint8_t selected) {
  gfx->clear(COL_BG);
  txtCenter(lora_head, T(UI_MENU_TITLE), CX, HEAD_Y, COL_IVORY);
  rule(HEAD_Y + 14, COL_GOLD_DIM);
  const char *items[] = {T(UI_MENU_HELP), T(UI_MENU_SETTINGS)};
  for (uint8_t i = 0; i < 2; i++) {
    const int16_t top = (int16_t)(MENU_BUTTON_TOP + i * MENU_BUTTON_STEP);
    const bool isSelected = i == selected;
    const uint16_t border = isSelected ? COL_GOLD : COL_RULE;
    const uint16_t fill = isSelected
                              ? blend565(COL_BG, COL_GOLD_DIM, 48)
                              : blend565(COL_BG, COL_RULE, 150);
    gfx->fillRoundRect(MENU_BUTTON_X, top, MENU_BUTTON_W, MENU_BUTTON_H,
                       12, fill);
    gfx->drawRoundRect(MENU_BUTTON_X, top, MENU_BUTTON_W, MENU_BUTTON_H,
                       12, border);
    txtCenter(lora_label, items[i], CX, (int16_t)(top + 41),
              isSelected ? COL_GOLD : COL_IVORY, 3);
  }
  gfx->fillRoundRect(SETTINGS_BACK_X, SETTINGS_BACK_TOP, SETTINGS_BACK_W,
                     SETTINGS_BACK_H, 12, blend565(COL_BG, COL_RULE, 150));
  gfx->drawRoundRect(SETTINGS_BACK_X, SETTINGS_BACK_TOP, SETTINGS_BACK_W,
                     SETTINGS_BACK_H, 12, COL_GOLD_DIM);
  txtCenter(lora_label, T(UI_BACK_TO_DECK), CX,
            (int16_t)(SETTINGS_BACK_TOP + 33), COL_GOLD_DIM, 2);
  powerBadge();
}

void uiHelp() {
  gfx->clear(COL_BG);
  txtCenter(lora_head, T(UI_HELP_TITLE), CX, HEAD_Y, COL_IVORY);
  rule(HEAD_Y + 14, COL_GOLD_DIM);

  // The two draws are different rituals, not one with pieces removed, so the
  // help describes whichever is switched on. Settings is one screen away.
  const char *const THREE[] = {
      T(UI_HELP3_1),
      T(UI_HELP3_2),
      T(UI_HELP3_3),
      T(UI_HELP3_4),
      T(UI_HELP3_5),
  };
  const char *const ONE[] = {
      T(UI_HELP1_1),
      T(UI_HELP1_2),
      T(UI_HELP1_3),
      T(UI_HELP1_4),
      T(UI_HELP1_5),
  };
  const char *const *lines = appSettings.singleCard ? ONE : THREE;
  int16_t y = (int16_t)(HEAD_Y + 44);
  for (uint8_t i = 0; i < 5; i++) {
    y = txtWrappedFn(lora_body, lines[i], widthAt, y, 20, COL_IVORY, 4);
    y = (int16_t)(y + 9);
  }
  hint(T(UI_TAP_TO_GO_BACK));
}

// ---------------- Settings ----------------
// A row's shared shell: the tappable plate and its label.
static int16_t settingsRowTop(uint8_t i) {
  return (int16_t)(SETTINGS_BUTTON_TOP + i * SETTINGS_BUTTON_STEP);
}

// How many states a list row has, and where it stands. The dots say what the
// value alone cannot: that there is a third deck, or a second language.
static void optionDots(int16_t x, int16_t cy, uint8_t n, uint8_t active,
                       uint16_t col) {
  if (n < 2 || n > 8) return;
  for (uint8_t i = 0; i < n; i++) {
    const int16_t dx = (int16_t)(x + i * 8);
    if (i == active) gfx->fillCircle(dx, cy, 2, col);
    else gfx->drawCircle(dx, cy, 2, COL_RULE);
  }
}

uint8_t uiBrightnessAtX(int16_t x) {
  int32_t t = (int32_t)x - SLIDER_X;
  if (t < 0) t = 0;
  if (t > SLIDER_W) t = SLIDER_W;
  return (uint8_t)(BRIGHT_MIN + (t * (255 - BRIGHT_MIN) + SLIDER_W / 2) / SLIDER_W);
}

void uiSettings(const AppSettings &settings, uint8_t selected, bool resetArmed) {
  gfx->clear(COL_BG);
  txtCenter(lora_head, T(UI_SETTINGS_TITLE), CX, HEAD_Y, COL_IVORY);
  rule(HEAD_Y + 14, COL_GOLD_DIM);

  const char *labels[] = {T(UI_SET_DECK), T(UI_SET_BRIGHTNESS),
                          T(UI_SET_LANGUAGE), T(UI_SET_RESET)};
  static_assert(sizeof labels / sizeof labels[0] == SETTINGS_ROWS, "row count");

  char value[64];
  for (uint8_t i = 0; i < SETTINGS_ROWS; i++) {
    const int16_t top = settingsRowTop(i);
    const int16_t labelY = (int16_t)(top + 20);
    const int16_t valueY = (int16_t)(top + 42);
    const bool isSelected = i == selected;
    const uint16_t border = isSelected ? COL_GOLD : COL_RULE;
    const uint16_t fill = isSelected
                              ? blend565(COL_BG, COL_GOLD_DIM, 48)
                              : blend565(COL_BG, COL_RULE, 150);
    gfx->fillRoundRect(SETTINGS_BUTTON_X, top, SETTINGS_BUTTON_W,
                       SETTINGS_BUTTON_H, 12, fill);
    gfx->drawRoundRect(SETTINGS_BUTTON_X, top, SETTINGS_BUTTON_W,
                       SETTINGS_BUTTON_H, 12, border);

    const uint16_t labelCol = isSelected ? COL_GOLD : COL_DIM;
    txtDraw(lora_small, labels[i], (int16_t)(SETTINGS_BUTTON_X + ROW_PAD),
            labelY, labelCol, -1, 2);
    const int16_t dotsX = (int16_t)(SETTINGS_BUTTON_X + ROW_PAD +
                                    txtWidth(lora_small, labels[i], -1, 2) + 14);
    const int16_t valueRight = (int16_t)(SETTINGS_BUTTON_X + SETTINGS_BUTTON_W - ROW_PAD);

    if (i == SET_ROW_BRIGHT) {
      // Direct manipulation: the track is the control, and the finger lands on
      // the level it wants. Cycling through four steps could only ever go one
      // way, so dimming meant a trip through full brightness.
      const int16_t ty = (int16_t)(top + 34);
      const int32_t span = (settings.brightness > BRIGHT_MIN)
                               ? (settings.brightness - BRIGHT_MIN)
                               : 0;
      const int16_t kx = (int16_t)(SLIDER_X + span * SLIDER_W / (255 - BRIGHT_MIN));
      gfx->fillRoundRect(SLIDER_X, (int16_t)(ty - 2), SLIDER_W, 5, 2, COL_RULE);
      gfx->fillRoundRect(SLIDER_X, (int16_t)(ty - 2), (int16_t)(kx - SLIDER_X), 5, 2,
                         isSelected ? COL_GOLD : COL_GOLD_DIM);
      gfx->fillCircle(kx, ty, 8, isSelected ? COL_GOLD : COL_GOLD_DIM);
      gfx->fillCircle(kx, ty, 5, COL_BG);
      snprintf(value, sizeof value, "%u%%",
               (unsigned int)((settings.brightness * 100u + 127u) / 255u));
      txtRight(lora_label, value, valueRight, valueY, COL_IVORY, 2);
      continue;
    }

    if (i == SET_ROW_DECK) {
      snprintf(value, sizeof value, "%s", deckText.name);
      optionDots(dotsX, (int16_t)(labelY - 4), DECK_COUNT, settings.deckId,
                 labelCol);
    } else if (i == SET_ROW_LANG) {
      // The language names itself, so the row reads in the language it selects.
      snprintf(value, sizeof value, "%s", T(UI_LANGUAGE_NAME));
      optionDots(dotsX, (int16_t)(labelY - 4), langCount(),
                 langIndexOf(settings.lang), labelCol);
    } else {
      // Reset asks twice. One stray tap must not take the deck, the language
      // and the brightness with it.
      snprintf(value, sizeof value, "%s",
               resetArmed ? T(UI_RESET_ARMED) : T(UI_RESET_VALUE));
    }
    txtRight(lora_label, value,
             valueRight, valueY,
             (i == SET_ROW_RESET && resetArmed) ? COL_GOLD : COL_IVORY, 2);

    // A swatch of the deck's own artwork, left of its name. The names say
    // which deck; only the picture says what it looks like, which is the
    // reason to switch. Card 0 because every deck has one.
    //
    // cardDrawFaceScaled() falls back to a full-size card when the bitmap is
    // missing, which would paint over the whole screen from inside a 52 px
    // row, so ask for the bitmap first and skip the swatch without one.
    if (i == SET_ROW_DECK && cardBitmap(0, CARD_L)) {
      const int16_t pw = 20, ph = 36;
      const int16_t vx = (int16_t)(valueRight - txtWidth(lora_label, value, -1, 2));
      cardDrawFaceScaled(0, (int16_t)(vx - 12 - pw / 2),
                         (int16_t)(top + (SETTINGS_BUTTON_H - ph) / 2), pw, ph);
    }
  }

  // A real on-screen back target keeps the screen usable without the case
  // buttons. Tapping any other area still backs out for backwards behavior.
  gfx->fillRoundRect(SETTINGS_BACK_X, SETTINGS_BACK_TOP, SETTINGS_BACK_W,
                     SETTINGS_BACK_H, 12, blend565(COL_BG, COL_RULE, 150));
  gfx->drawRoundRect(SETTINGS_BACK_X, SETTINGS_BACK_TOP, SETTINGS_BACK_W,
                     SETTINGS_BACK_H, 12, COL_GOLD_DIM);
  txtCenter(lora_label, T(UI_BACK_TO_MENU), CX,
            (int16_t)(SETTINGS_BACK_TOP + 33), COL_GOLD_DIM, 2);
  txtCenter(lora_small, T(UI_TAP_AN_OPTION), CX, 447, COL_GOLD_DIM, 1);
}

// ---------------- Cut and deal ----------------

void uiDeal(float p, uint8_t count) {
  gfx->clear(COL_BG);
  // No deck under the spread: the first card is the deck, and the moment
  // it moves the stack behind it is gone.
  // One card goes straight to the size and place the big view uses, so the
  // hand-off is invisible. Three cards go to their slots at spread size.
  const CardSize sz = count == 1 ? CARD_L : CARD_S;
  const int16_t endY = count == 1 ? (int16_t)((SCR_H - CARD_H[CARD_L]) / 2) : SLOT_Y;
  for (uint8_t i = 0; i < count; i++) {
    const float start = i * 0.22f;
    const float u = easeOut((p - start) / 0.5f);
    if (u <= 0) continue;
    const int16_t cx = (int16_t)(DECK_CX + (slotCx(count, i) - DECK_CX) * u);
    const int16_t y = (int16_t)(DECK_Y + (endY - DECK_Y) * u);
    const int16_t w = (int16_t)(DECK_W + (CARD_W[sz] - DECK_W) * u);
    const int16_t h = (int16_t)(DECK_H + (CARD_H[sz] - DECK_H) * u);
    cardDrawBack(cx, y, w, h, 1.0f, 0.5f * (1 - u) + 0.15f);
  }
}

void uiGather(float p, uint8_t count) {
  gfx->clear(COL_BG);
  // Future returns first, then present, then past lands on top as the deck.
  for (int8_t i = (int8_t)count - 1; i >= 0; i--) {
    const float start = (count - 1 - i) * 0.22f;
    const float u = easeOut((p - start) / 0.5f);
    const int16_t sx = slotCx(count, (uint8_t)i);
    const CardSize sz = count == 1 ? CARD_L : CARD_S;
    const int16_t fromY = count == 1 ? (int16_t)((SCR_H - CARD_H[CARD_L]) / 2) : SLOT_Y;
    const int16_t cx = (int16_t)(sx + (DECK_CX - sx) * u);
    const int16_t y = (int16_t)(fromY + (DECK_Y - fromY) * u);
    const int16_t w = (int16_t)(CARD_W[sz] + (DECK_W - CARD_W[sz]) * u);
    const int16_t h = (int16_t)(CARD_H[sz] + (DECK_H - CARD_H[sz]) * u);
    cardDrawBack(cx, y, w, h, 1.0f, 0.15f + 0.25f * u);
  }
}

// ---------------- Spread ----------------
static void drawSlotLabel(uint8_t count, uint8_t i, uint16_t col) {
  // A single card carries no position label: there is no past to contrast it
  // with, and "PRESENT" over a lone card states the obvious.
  if (count == 1) return;
  // Wide enough for a translated position name in UTF-8, where an accented
  // letter costs two bytes.
  char buf[32];
  upper(buf, sizeof buf, T((uint16_t)(ENG_POS_PAST + i)));
  txtCenter(lora_small, buf, SLOT_CX[i], SLOT_Y - 12, col, 2);
}

static void spreadBase(const Spread &s, int8_t flipping, float flipPhase,
                       int8_t hide, bool resetArmed = false);

void uiSpread(const Spread &s, int8_t flipping, float flipPhase, bool resetArmed) {
  spreadBase(s, flipping, flipPhase, -1, resetArmed);
}

void uiZoom(const Spread &s, uint8_t pos, float p) {
  spreadBase(s, -1, 0, (int8_t)pos, false);
  const float u = easeOut(p);
  const int16_t bigY = (int16_t)((SCR_H - CARD_H[CARD_L]) / 2);
  const int16_t from = slotCx(s.count, pos);
  const int16_t cx = (int16_t)(from + (CX - from) * u);
  const int16_t y = (int16_t)(SLOT_Y + (bigY - SLOT_Y) * u);
  const int16_t w = (int16_t)(CARD_W[CARD_S] + (CARD_W[CARD_L] - CARD_W[CARD_S]) * u);
  const int16_t h = (int16_t)(CARD_H[CARD_S] + (CARD_H[CARD_L] - CARD_H[CARD_S]) * u);
  cardDrawFaceScaled(s.reading.card[pos], cx, y, w, h);
}

static void spreadBase(const Spread &s, int8_t flipping, float flipPhase,
                       int8_t hide, bool resetArmed) {
  gfx->clear(COL_BG);
  uint8_t n = 0;
  for (uint8_t i = 0; i < s.count; i++) if (s.revealed[i]) n++;
  const bool done = n == s.count;
  txtCenter(lora_head,
            done ? T(UI_YOUR_READING) : (s.count == 1 ? T(UI_TURN_THE_CARD) : T(UI_TURN_THE_CARDS)),
            CX, HEAD_Y, COL_IVORY);
  rule(HEAD_Y + 14, COL_RULE);

  const int16_t cardW = CARD_W[CARD_S], cardH = CARD_H[CARD_S];
  for (uint8_t i = 0; i < s.count; i++) {
    const int16_t sx = slotCx(s.count, i);
    drawSlotLabel(s.count, i, s.revealed[i] ? COL_GOLD : COL_GOLD_DIM);
    const uint8_t idx = s.reading.card[i];
    if ((int8_t)i == hide) {
      continue;
    } else if ((int8_t)i == flipping) {
      if (flipPhase < 0.5f) {
        cardDrawBack(sx, SLOT_Y, cardW, cardH, 1.0f - flipPhase * 2.0f, 0.6f);
      } else {
        cardDrawFace(idx, CARD_S, sx, SLOT_Y, (flipPhase - 0.5f) * 2.0f);
      }
    } else if (s.revealed[i]) {
      cardDrawFace(idx, CARD_S, sx, SLOT_Y, 1.0f);
    } else {
      cardDrawBack(sx, SLOT_Y, cardW, cardH, 1.0f, 0.15f);
    }
    if (s.revealed[i] && (int8_t)i != flipping) {
      const DeckDefinition &deck = deckById(s.deck);
      txtWrapped(lora_small, deckCard(deckText, idx).name,
                 (int16_t)(sx - cardW / 2 - 2),
                 (int16_t)(SLOT_Y + cardH + 21), (int16_t)(cardW + 4), 16, COL_IVORY, 2, true);
    }
  }

  if (s.count == 3 && n == 3) {
    const DeckDefinition &deck = deckById(s.deck);
    const uint8_t h = tarotHiddenCard(deck, s.reading);
    char buf[96];
    strFormat(buf, sizeof buf, T(UI_BENEATH_THEM), deckCard(deckText, h).name);
#if UI_ROUND
    // 226 px at its longest ("Beneath them: The High Priestess"), so it needs
    // a chord of at least that: 384 gives 318.
    txtCenter(lora_italic, buf, CX, 384, COL_DIM);
#else
    txtCenter(lora_italic, buf, CX, 380, COL_DIM);
#endif
  }
  // The hint stands only until the first card turns. After that the spread
  // says what to do by looking like a spread, and the reader has already
  // proved they know how.
  //
  // Armed, the reset takes the hint's line: the warning belongs where the
  // instructions were, and there is no other clear row this low on the face.
  if (resetArmed) hint(T(UI_RESET_ARMED));
  else if (n == 0) hint(s.count == 1 ? T(UI_TAP_THE_CARD) : T(UI_TAP_A_CARD));

  // Closing the reading. The cards are not saved anywhere, so the first tap
  // only arms it and says so; the second gathers them back into the deck.
  mark(CX, RESET_CY, true, resetArmed ? COL_GOLD : COL_GOLD_DIM);
  if (resetArmed) gfx->drawCircle(CX, RESET_CY, (int16_t)(MARK_R + 4), COL_GOLD_DIM);
}

// ---------------- One card, large ----------------
void uiCardBig(const Spread &s, uint8_t pos) {
  gfx->clear(COL_BG);
  const uint8_t idx = s.reading.card[pos];
  const int16_t y = (int16_t)((SCR_H - CARD_H[CARD_L]) / 2);
  const bool up = s.revealed[pos];
  if (up) cardDrawFace(idx, CARD_L, CX, y, 1.0f);
  else cardDrawBack(CX, y, CARD_W[CARD_L], CARD_H[CARD_L], 1.0f, 0.2f);
#if UI_ROUND
  if (s.count != 1) {
    char label[32];
    upper(label, sizeof label, T((uint16_t)(ENG_POS_PAST + slotTextPos(s.count, pos))));
    txtCenter(lora_small, label, CX, 26, COL_GOLD, 3);
  }
  txtCenter(lora_small, up ? T(UI_TAP_TO_READ) : T(UI_TAP_TO_TURN), CX, HINT_Y, COL_DIM, 1);
#endif
}

// The single-card turn, at full size. The spread's flip squashes a small card
// in its slot; this does the same thing in the middle of the screen, because
// a one-card draw never visits the spread.
void uiCardFlip(const Spread &s, uint8_t pos, float phase) {
  gfx->clear(COL_BG);
  const uint8_t idx = s.reading.card[pos];
  const int16_t y = (int16_t)((SCR_H - CARD_H[CARD_L]) / 2);
  if (phase < 0.5f)
    cardDrawBack(CX, y, CARD_W[CARD_L], CARD_H[CARD_L], 1.0f - phase * 2.0f, 0.6f);
  else
    cardDrawFace(idx, CARD_L, CX, y, (phase - 0.5f) * 2.0f);
}

// ---------------- Meaning ----------------
void uiMeaning(const Spread &s, uint8_t pos) {
  gfx->clear(COL_BG);
  const uint8_t idx = s.reading.card[pos];
  const DeckDefinition &deck = deckById(s.deck);
  const CardInfo &c = deckCard(deckText, idx);
  char label[32];
  upper(label, sizeof label, T((uint16_t)(ENG_POS_PAST + slotTextPos(s.count, pos))));
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
  const int16_t after = txtWrappedFn(lora_meaning, cardPositionText(c, slotTextPos(s.count, pos)), meaningWidthAt, bodyY, lineH, COL_IVORY, 6);

  // The card's Golden Dawn glyph, with numeral, attribution and element,
  // hung a fixed distance under the last line so the page reads as one block.
  // scripts/preview_read.py BUMPED2 mirrors these numbers.
  int16_t glyphY = (int16_t)(after - lineH + 50);
  const int16_t glyphMaxY = DOTS_Y - 64;
  if (glyphY > glyphMaxY) glyphY = glyphMaxY;
  if (deck.glyphs) glyphDraw(deck.glyphs[idx], CX, glyphY, 46, COL_GOLD);
  char cap[96];
  char ruler[40], el[24];
  upper(ruler, sizeof ruler, c.ruler);
  upper(el, sizeof el, T((uint16_t)((uint16_t)ENG_EL_FIRE + c.element)));
  if (!ruler[0] || strcmp(ruler, el) == 0)
    snprintf(cap, sizeof cap, "%s   %s", c.numeral, el);
  else snprintf(cap, sizeof cap, "%s   %s   %s", c.numeral, ruler, el);
  txtCenter(lora_label, cap, CX, (int16_t)(glyphY + 44), COL_GOLD_DIM, 2);

  // No hint here. The attribution sits at glyphY + 44 and the two hint lines
  // sat at 380 and 404, so all three shared two rows and overprinted. This is
  // a reading page: the dots carry position, the menu documents the controls.
  dots(3, pos, DOTS_Y);
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
  txtCenter(lora_head, T(UI_INNER_TITLE), CX, headY, COL_IVORY);
  char sub[96];
  snprintf(sub, sizeof sub, "%s  /  %s  /  %s",
           deckCard(deckText, s.reading.card[0]).name,
           deckCard(deckText, s.reading.card[1]).name,
           deckCard(deckText, s.reading.card[2]).name);
  int16_t x0 = 0;
  if (txtWidth(lora_small, sub) > widthAt(subY, &x0))
    snprintf(sub, sizeof sub, "%s / %s / %s",
             deckCard(deckText, s.reading.card[0]).numeral,
             deckCard(deckText, s.reading.card[1]).numeral,
             deckCard(deckText, s.reading.card[2]).numeral);
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
  if (x < MENU_BUTTON_X || x >= MENU_BUTTON_X + MENU_BUTTON_W) return -1;
  for (int8_t i = 0; i < 2; i++) {
    const int16_t top = (int16_t)(MENU_BUTTON_TOP + i * MENU_BUTTON_STEP);
    if (y >= top && y < top + MENU_BUTTON_H) return i;
  }
  return -1;
}

int8_t uiDrawModeHit(int16_t x, int16_t y) {
  if (y < DRAW_SEG_TOP - TOUCH_PAD || y >= DRAW_SEG_TOP + DRAW_SEG_H + TOUCH_PAD)
    return -1;
  for (int8_t i = 0; i < 2; i++) {
    const int16_t sx = drawSegX((uint8_t)i);
    if (x >= sx - TOUCH_PAD && x < sx + DRAW_SEG_W + TOUCH_PAD) return i;
  }
  return -1;
}

bool uiReadingResetHit(int16_t x, int16_t y) {
  const int16_t r = (int16_t)(MARK_R + TOUCH_PAD);
  return x >= CX - r && x <= CX + r && y >= RESET_CY - r && y <= RESET_CY + r;
}

bool uiSettingsLinkHit(int16_t x, int16_t y) {
  // Padded on three sides only. The bottom edge stops at the deck artwork at
  // 108, so the pad never takes a touch that was meant for the cards.
  return x >= LINK_X - TOUCH_PAD && x < LINK_X + LINK_W + TOUCH_PAD &&
         y >= LINK_TOP - TOUCH_PAD && y < LINK_TOP + LINK_H + TOUCH_PAD / 2;
}

int8_t uiSettingsHit(int16_t x, int16_t y) {
  if (x < SETTINGS_BUTTON_X || x >= SETTINGS_BUTTON_X + SETTINGS_BUTTON_W) return -1;
  for (int8_t i = 0; i < (int8_t)SETTINGS_ROWS; i++) {
    const int16_t top = settingsRowTop((uint8_t)i);
    if (y >= top && y < top + SETTINGS_BUTTON_H) return i;
  }
  return -1;
}

int8_t uiSlotHit(const Spread &s, int16_t x, int16_t y) {
  if (y < SLOT_Y - 24 || y > SLOT_Y + CARD_H[CARD_S] + 40) return -1;
  for (int8_t i = 0; i < (int8_t)s.count; i++) {
    const int16_t cx = slotCx(s.count, (uint8_t)i);
    if (x >= cx - CARD_W[CARD_S] / 2 - 7 && x <= cx + CARD_W[CARD_S] / 2 + 7) return i;
  }
  return -1;
}
