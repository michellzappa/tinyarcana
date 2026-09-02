#include "glyphs.h"

#include <math.h>

#include "board_display.h"
#include "text.h"

// Each glyph is a list of strokes in a unit box, y down. Rendering is a
// signed distance field per pixel, so the strokes come out anti-aliased at
// any size. scripts/preview_glyphs.py parses this same table.
enum StrokeType : uint8_t { S_END = 0, S_SEG, S_ARC, S_DISC };

struct Stroke {
  uint8_t t;
  float a, b, c, d, e;   // SEG: x0 y0 x1 y1 -  ARC: cx cy r a0 a1 (deg)  DISC: cx cy r
};

#define SEG(x0, y0, x1, y1) {S_SEG, x0, y0, x1, y1, 0}
#define ARC(cx, cy, r, a0, a1) {S_ARC, cx, cy, r, a0, a1}
#define CIRC(cx, cy, r) {S_ARC, cx, cy, r, 0, 360}
#define DISC(cx, cy, r) {S_DISC, cx, cy, r, 0, 0}
#define END {S_END, 0, 0, 0, 0, 0}

// GLYPH_TABLE_BEGIN
static const Stroke STROKES[] = {
  // G_FIRE
  SEG(0.50f, 0.10f, 0.90f, 0.86f), SEG(0.90f, 0.86f, 0.10f, 0.86f), SEG(0.10f, 0.86f, 0.50f, 0.10f), END,
  // G_WATER
  SEG(0.10f, 0.14f, 0.90f, 0.14f), SEG(0.90f, 0.14f, 0.50f, 0.90f), SEG(0.50f, 0.90f, 0.10f, 0.14f), END,
  // G_AIR
  SEG(0.50f, 0.10f, 0.90f, 0.86f), SEG(0.90f, 0.86f, 0.10f, 0.86f), SEG(0.10f, 0.86f, 0.50f, 0.10f), SEG(0.20f, 0.62f, 0.80f, 0.62f), END,
  // G_EARTH
  SEG(0.10f, 0.14f, 0.90f, 0.14f), SEG(0.90f, 0.14f, 0.50f, 0.90f), SEG(0.50f, 0.90f, 0.10f, 0.14f), SEG(0.20f, 0.38f, 0.80f, 0.38f), END,
  // G_SUN
  CIRC(0.50f, 0.50f, 0.38f), DISC(0.50f, 0.50f, 0.07f), END,
  // G_MOON
  ARC(0.50f, 0.50f, 0.40f, 55, 305), ARC(0.78f, 0.50f, 0.42f, 129, 231), END,
  // G_MERCURY
  CIRC(0.50f, 0.44f, 0.19f), SEG(0.50f, 0.63f, 0.50f, 0.94f), SEG(0.35f, 0.79f, 0.65f, 0.79f), ARC(0.50f, 0.06f, 0.19f, 25, 155), END,
  // G_VENUS
  CIRC(0.50f, 0.36f, 0.25f), SEG(0.50f, 0.61f, 0.50f, 0.95f), SEG(0.34f, 0.80f, 0.66f, 0.80f), END,
  // G_MARS
  CIRC(0.42f, 0.60f, 0.27f), SEG(0.61f, 0.41f, 0.90f, 0.12f), SEG(0.90f, 0.12f, 0.64f, 0.12f), SEG(0.90f, 0.12f, 0.90f, 0.38f), END,
  // G_JUPITER
  ARC(0.36f, 0.30f, 0.15f, 180, 330), SEG(0.49f, 0.22f, 0.20f, 0.66f), SEG(0.20f, 0.66f, 0.82f, 0.66f), SEG(0.64f, 0.10f, 0.64f, 0.92f), END,
  // G_SATURN
  SEG(0.34f, 0.08f, 0.34f, 0.72f), SEG(0.18f, 0.24f, 0.50f, 0.24f), ARC(0.55f, 0.52f, 0.21f, 180, 360), SEG(0.76f, 0.52f, 0.76f, 0.70f), ARC(0.66f, 0.70f, 0.10f, 0, 120), END,
  // G_ARIES
  SEG(0.50f, 0.36f, 0.50f, 0.92f), ARC(0.30f, 0.36f, 0.20f, 180, 360), ARC(0.70f, 0.36f, 0.20f, 180, 360), END,
  // G_TAURUS
  CIRC(0.50f, 0.62f, 0.27f), ARC(0.50f, 0.06f, 0.30f, 20, 160), END,
  // G_GEMINI
  SEG(0.35f, 0.18f, 0.35f, 0.82f), SEG(0.65f, 0.18f, 0.65f, 0.82f), ARC(0.50f, 0.42f, 0.36f, 215, 325), ARC(0.50f, 0.58f, 0.36f, 35, 145), END,
  // G_CANCER
  CIRC(0.28f, 0.40f, 0.11f), ARC(0.50f, 0.60f, 0.36f, 210, 330), CIRC(0.72f, 0.60f, 0.11f), ARC(0.50f, 0.40f, 0.36f, 30, 150), END,
  // G_LEO
  CIRC(0.28f, 0.72f, 0.13f), SEG(0.36f, 0.62f, 0.34f, 0.52f), ARC(0.52f, 0.40f, 0.22f, 150, 380), SEG(0.727f, 0.475f, 0.70f, 0.78f), ARC(0.78f, 0.78f, 0.08f, 90, 180), END,
  // G_VIRGO
  SEG(0.13f, 0.30f, 0.13f, 0.80f), ARC(0.255f, 0.30f, 0.125f, 180, 360), SEG(0.38f, 0.30f, 0.38f, 0.80f), ARC(0.505f, 0.30f, 0.125f, 180, 360), SEG(0.63f, 0.30f, 0.63f, 0.62f), ARC(0.63f, 0.76f, 0.14f, 180, 450), SEG(0.63f, 0.90f, 0.52f, 0.96f), END,
  // G_LIBRA
  SEG(0.10f, 0.86f, 0.90f, 0.86f), SEG(0.10f, 0.64f, 0.32f, 0.64f), ARC(0.50f, 0.64f, 0.18f, 180, 360), SEG(0.68f, 0.64f, 0.90f, 0.64f), END,
  // G_SCORPIO
  SEG(0.13f, 0.30f, 0.13f, 0.80f), ARC(0.255f, 0.30f, 0.125f, 180, 360), SEG(0.38f, 0.30f, 0.38f, 0.80f), ARC(0.505f, 0.30f, 0.125f, 180, 360), SEG(0.63f, 0.30f, 0.63f, 0.74f), ARC(0.75f, 0.74f, 0.12f, 0, 180), SEG(0.87f, 0.74f, 0.87f, 0.56f), SEG(0.87f, 0.56f, 0.79f, 0.64f), SEG(0.87f, 0.56f, 0.95f, 0.64f), END,
  // G_SAGITTARIUS
  SEG(0.14f, 0.86f, 0.86f, 0.14f), SEG(0.86f, 0.14f, 0.56f, 0.14f), SEG(0.86f, 0.14f, 0.86f, 0.44f), SEG(0.30f, 0.50f, 0.50f, 0.70f), END,
  // G_CAPRICORN
  SEG(0.10f, 0.30f, 0.10f, 0.66f), ARC(0.24f, 0.30f, 0.14f, 180, 360), SEG(0.38f, 0.30f, 0.40f, 0.62f), SEG(0.40f, 0.62f, 0.56f, 0.52f), CIRC(0.66f, 0.66f, 0.15f), END,
  // G_AQUARIUS
  SEG(0.08f, 0.42f, 0.22f, 0.28f), SEG(0.22f, 0.28f, 0.36f, 0.42f), SEG(0.36f, 0.42f, 0.50f, 0.28f), SEG(0.50f, 0.28f, 0.64f, 0.42f), SEG(0.64f, 0.42f, 0.78f, 0.28f), SEG(0.78f, 0.28f, 0.92f, 0.42f),
  SEG(0.08f, 0.72f, 0.22f, 0.58f), SEG(0.22f, 0.58f, 0.36f, 0.72f), SEG(0.36f, 0.72f, 0.50f, 0.58f), SEG(0.50f, 0.58f, 0.64f, 0.72f), SEG(0.64f, 0.72f, 0.78f, 0.58f), SEG(0.78f, 0.58f, 0.92f, 0.72f), END,
  // G_PISCES
  ARC(0.08f, 0.50f, 0.36f, -58, 58), ARC(0.92f, 0.50f, 0.36f, 122, 238), SEG(0.22f, 0.50f, 0.78f, 0.50f), END,
};
// GLYPH_TABLE_END

const Glyph CARD_GLYPH[22] = {
  G_AIR,        // 0  Fool
  G_MERCURY,    // 1  Magician
  G_MOON,       // 2  High Priestess
  G_VENUS,      // 3  Empress
  G_ARIES,      // 4  Emperor
  G_TAURUS,     // 5  Hierophant
  G_GEMINI,     // 6  Lovers
  G_CANCER,     // 7  Chariot
  G_LEO,        // 8  Strength
  G_VIRGO,      // 9  Hermit
  G_JUPITER,    // 10 Wheel of Fortune
  G_LIBRA,      // 11 Justice
  G_WATER,      // 12 Hanged Man
  G_SCORPIO,    // 13 Death
  G_SAGITTARIUS,// 14 Temperance
  G_CAPRICORN,  // 15 Devil
  G_MARS,       // 16 Tower
  G_AQUARIUS,   // 17 Star
  G_PISCES,     // 18 Moon
  G_SUN,        // 19 Sun
  G_FIRE,       // 20 Judgement
  G_SATURN,     // 21 World
};

static const Stroke *glyphStart(Glyph g) {
  const Stroke *s = STROKES;
  for (uint8_t i = 0; i < g; i++) {
    while (s->t != S_END) s++;
    s++;
  }
  return s;
}

static float segDist(float px, float py, float x0, float y0, float x1, float y1) {
  const float dx = x1 - x0, dy = y1 - y0;
  const float l2 = dx * dx + dy * dy;
  float t = l2 > 0 ? ((px - x0) * dx + (py - y0) * dy) / l2 : 0;
  if (t < 0) t = 0; else if (t > 1) t = 1;
  const float ex = px - (x0 + t * dx), ey = py - (y0 + t * dy);
  return sqrtf(ex * ex + ey * ey);
}

static float arcDist(float px, float py, float cx, float cy, float r, float a0, float a1) {
  const float dx = px - cx, dy = py - cy;
  const float d = sqrtf(dx * dx + dy * dy);
  if (a1 - a0 >= 360) return fabsf(d - r);
  float ang = atan2f(dy, dx) * 57.2957795f;
  // Normalise into [a0, a0 + 360).
  while (ang < a0) ang += 360;
  while (ang >= a0 + 360) ang -= 360;
  if (ang <= a1) return fabsf(d - r);
  // Off the arc: nearest endpoint.
  const float r0 = a0 * 0.01745329f, r1 = a1 * 0.01745329f;
  const float ex0 = px - (cx + r * cosf(r0)), ey0 = py - (cy + r * sinf(r0));
  const float ex1 = px - (cx + r * cosf(r1)), ey1 = py - (cy + r * sinf(r1));
  const float d0 = sqrtf(ex0 * ex0 + ey0 * ey0), d1 = sqrtf(ex1 * ex1 + ey1 * ey1);
  return d0 < d1 ? d0 : d1;
}

void glyphDraw(Glyph g, int16_t cx, int16_t cy, int16_t size, uint16_t col) {
  if (g >= G_COUNT) return;
  uint16_t *fb = gfx->fb();
  if (!fb) return;
  const Stroke *first = glyphStart(g);
  const float hw = size / 32.0f;          // half stroke width in pixels
  const int16_t x0 = (int16_t)(cx - size / 2), y0 = (int16_t)(cy - size / 2);
  for (int16_t j = 0; j < size; j++) {
    const int16_t py = (int16_t)(y0 + j);
    if (py < 0 || py >= SCR_H) continue;
    uint16_t *row = fb + (int32_t)py * SCR_W;
    const float uy = (j + 0.5f) / size;
    for (int16_t i = 0; i < size; i++) {
      const int16_t px = (int16_t)(x0 + i);
      if (px < 0 || px >= SCR_W) continue;
      const float ux = (i + 0.5f) / size;
      float best = 1e9f;
      for (const Stroke *s = first; s->t != S_END; s++) {
        float d;
        if (s->t == S_SEG) d = segDist(ux, uy, s->a, s->b, s->c, s->d) * size;
        else if (s->t == S_ARC) d = arcDist(ux, uy, s->a, s->b, s->c, s->d, s->e) * size;
        else {
          const float dx = ux - s->a, dy = uy - s->b;
          d = (sqrtf(dx * dx + dy * dy) - s->c) * size;
          if (d < 0) d = 0;
          d -= hw;   // discs are filled: pull the edge in so the rim is crisp
        }
        if (d < best) best = d;
      }
      float a = hw + 0.5f - best;
      if (a <= 0) continue;
      if (a > 1) a = 1;
      row[px] = blend565(row[px], col, (uint8_t)(a * 255));
    }
  }
}
