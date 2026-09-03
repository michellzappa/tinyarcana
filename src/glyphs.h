// Astrological and alchemical glyphs, one per major, drawn from stroke
// descriptions so the repository carries no symbol font. Attributions are
// the Golden Dawn ones the Rider-Waite-Smith deck was designed around:
// twelve zodiac signs, seven planets, and the three elemental triangles
// for the Fool (air), the Hanged Man (water) and Judgement (fire).
#pragma once

#include <Arduino.h>

enum Glyph : uint8_t {
  G_FIRE = 0, G_WATER, G_AIR, G_EARTH,
  G_SUN, G_MOON, G_MERCURY, G_VENUS, G_MARS, G_JUPITER, G_SATURN,
  G_ARIES, G_TAURUS, G_GEMINI, G_CANCER, G_LEO, G_VIRGO,
  G_LIBRA, G_SCORPIO, G_SAGITTARIUS, G_CAPRICORN, G_AQUARIUS, G_PISCES,
  G_COUNT
};

// Draw centred on (cx, cy) inside a size x size box, stroke ~size/16.
void glyphDraw(Glyph g, int16_t cx, int16_t cy, int16_t size, uint16_t col);
