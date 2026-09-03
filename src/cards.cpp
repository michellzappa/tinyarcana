#include "cards.h"

#include <LittleFS.h>
#include <esp_heap_caps.h>
#include <math.h>

#include "board_display.h"
#include "text.h"

static bool fsOk = false;
static uint16_t *cache[22][2] = {};

bool cardsBegin() {
  fsOk = LittleFS.begin(false);
  if (!fsOk) {
    Serial.println("littlefs: mount FAIL (run: pio run -t uploadfs)");
    return false;
  }
  Serial.printf("littlefs: %u/%u KB\n", (unsigned)(LittleFS.usedBytes() / 1024),
                (unsigned)(LittleFS.totalBytes() / 1024));
  char path[32];
  snprintf(path, sizeof path, "/cards/00_%dx%d.565", CARD_W[CARD_S], CARD_H[CARD_S]);
  return LittleFS.exists(path);
}

bool cardsReady() { return fsOk; }

const uint16_t *cardBitmap(uint8_t idx, CardSize sz) {
  if (idx > 21 || !fsOk) return nullptr;
  if (cache[idx][sz]) return cache[idx][sz];
  char path[32];
  snprintf(path, sizeof path, "/cards/%02u_%dx%d.565", idx, CARD_W[sz], CARD_H[sz]);
  File f = LittleFS.open(path, "r");
  if (!f) {
    Serial.printf("cards: missing %s\n", path);
    return nullptr;
  }
  const size_t bytes = (size_t)CARD_W[sz] * CARD_H[sz] * 2;
  uint16_t *buf = (uint16_t *)ps_malloc(bytes);
  if (!buf) { f.close(); return nullptr; }
  const size_t got = f.read((uint8_t *)buf, bytes);
  f.close();
  if (got != bytes) {
    Serial.printf("cards: short read %s (%u/%u)\n", path, (unsigned)got, (unsigned)bytes);
    free(buf);
    return nullptr;
  }
  cache[idx][sz] = buf;
  return buf;
}

void cardPreload(uint8_t idx) {
  cardBitmap(idx, CARD_S);
  cardBitmap(idx, CARD_L);
}

// Inside a rounded rect of w x h with corner radius r?
static inline bool insideRounded(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r) {
  int16_t cx = x < r ? r : (x >= w - r ? w - r - 1 : x);
  int16_t cy = y < r ? r : (y >= h - r ? h - r - 1 : y);
  if (cx == x && cy == y) return true;
  const int32_t dx = x - cx, dy = y - cy;
  return dx * dx + dy * dy <= (int32_t)r * r;
}

static const uint16_t COL_EDGE = RGB565(70, 62, 78);

void cardDrawFace(uint8_t idx, CardSize sz, int16_t cx, int16_t y, float squash) {
  const uint16_t *bmp = cardBitmap(idx, sz);
  uint16_t *fb = gfx->fb();
  if (!fb) return;
  const int16_t w = CARD_W[sz], h = CARD_H[sz];
  if (squash < 0.02f) squash = 0.02f;
  if (squash > 1.0f) squash = 1.0f;
  const int16_t dw = (int16_t)(w * squash + 0.5f);
  if (dw < 2) return;
  const int16_t x0 = (int16_t)(cx - dw / 2);
  const int16_t r = sz == CARD_S ? 6 : 10;
  const int16_t rr = (int16_t)(r * squash);
  for (int16_t dy = 0; dy < h; dy++) {
    const int16_t py = (int16_t)(y + dy);
    if (py < 0 || py >= SCR_H) continue;
    uint16_t *row = fb + (int32_t)py * SCR_W;
    for (int16_t dx = 0; dx < dw; dx++) {
      const int16_t px = (int16_t)(x0 + dx);
      if (px < 0 || px >= SCR_W) continue;
      if (!insideRounded(dx, dy, dw, h, rr < 2 ? 2 : rr)) continue;
      const bool edge = dx == 0 || dx == dw - 1 || dy == 0 || dy == h - 1;
      if (edge || !bmp) {
        row[px] = bmp ? COL_EDGE : RGB565(40, 36, 48);
        continue;
      }
      const int16_t sx = (int16_t)((int32_t)dx * w / dw);
      row[px] = bmp[(int32_t)dy * w + sx];
    }
  }
}

void cardDrawFaceScaled(uint8_t idx, int16_t cx, int16_t y, int16_t w, int16_t h) {
  const uint16_t *bmp = cardBitmap(idx, CARD_L);
  uint16_t *fb = gfx->fb();
  if (!fb || !bmp || w < 2 || h < 2) return;
  const int16_t sw = CARD_W[CARD_L], sh = CARD_H[CARD_L];
  const int16_t x0 = (int16_t)(cx - w / 2);
  int16_t r = (int16_t)(w / 22);
  if (r < 3) r = 3;
  for (int16_t dy = 0; dy < h; dy++) {
    const int16_t py = (int16_t)(y + dy);
    if (py < 0 || py >= SCR_H) continue;
    uint16_t *row = fb + (int32_t)py * SCR_W;
    const uint16_t *src = bmp + (int32_t)((int32_t)dy * sh / h) * sw;
    for (int16_t dx = 0; dx < w; dx++) {
      const int16_t px = (int16_t)(x0 + dx);
      if (px < 0 || px >= SCR_W) continue;
      if (!insideRounded(dx, dy, w, h, r)) continue;
      if (dx == 0 || dx == w - 1 || dy == 0 || dy == h - 1) { row[px] = COL_EDGE; continue; }
      row[px] = src[(int32_t)dx * sw / w];
    }
  }
}

static inline uint16_t lerp565(uint16_t a, uint16_t b, float t) {
  if (t <= 0) return a;
  if (t >= 1) return b;
  return blend565(a, b, (uint8_t)(t * 255));
}

void cardDrawBack(int16_t cx, int16_t y, int16_t w, int16_t h, float squash,
                  float glow) {
  uint16_t *fb = gfx->fb();
  if (!fb) return;
  if (squash < 0.02f) squash = 0.02f;
  if (squash > 1.0f) squash = 1.0f;
  const int16_t dw = (int16_t)(w * squash + 0.5f);
  if (dw < 2) return;
  const int16_t x0 = (int16_t)(cx - dw / 2);
  const int16_t r = (int16_t)(w / 16);
  const int16_t rr = (int16_t)(r * squash);

  const uint16_t indigo = RGB565(24, 22, 56);
  const uint16_t indigoDeep = RGB565(16, 14, 40);
  const uint16_t goldDim = RGB565(96, 80, 46);
  const uint16_t goldHi = RGB565(226, 188, 104);
  const uint16_t gold = lerp565(RGB565(170, 138, 72), goldHi, glow);
  const uint16_t lattice = lerp565(RGB565(44, 40, 84), RGB565(70, 62, 110), glow);

  const int16_t mcx = (int16_t)(w / 2), mcy = (int16_t)(h / 2);
  const int16_t border = (int16_t)(w >= 120 ? 3 : 2);
  const int16_t frame = (int16_t)(border + w / 20);
  const int32_t ringR = w / 5;
  const int16_t pitch = (int16_t)(w >= 120 ? 18 : 14);

  // Two hoists worth having: this runs 39,600 times per card and three cards
  // per frame on the deck screen. Unsquashed, the source column equals the
  // destination column, so the per-pixel divide is pure waste; and the rounded
  // corners can only reject a pixel within the corner radius of the top or
  // bottom edge, so the rest of the rows need no test at all.
  const bool noSquash = (dw == w);
  const int16_t rr2 = rr < 2 ? 2 : rr;
  for (int16_t dy = 0; dy < h; dy++) {
    const int16_t py = (int16_t)(y + dy);
    if (py < 0 || py >= SCR_H) continue;
    uint16_t *row = fb + (int32_t)py * SCR_W;
    const bool cornerRow = (dy < rr2) || (dy >= h - rr2);
    for (int16_t dx = 0; dx < dw; dx++) {
      const int16_t px = (int16_t)(x0 + dx);
      if (px < 0 || px >= SCR_W) continue;
      if (cornerRow && !insideRounded(dx, dy, dw, h, rr2)) continue;
      const int16_t sx = noSquash ? dx : (int16_t)((int32_t)dx * w / dw);
      uint16_t c;
      if (sx < border || sx >= w - border || dy < border || dy >= h - border) {
        c = gold;
      } else if (sx == frame || sx == w - 1 - frame || dy == frame || dy == h - 1 - frame) {
        c = goldDim;
      } else if (sx < frame || sx >= w - frame || dy < frame || dy >= h - frame) {
        c = indigoDeep;
      } else {
        const int32_t ex = sx - mcx, ey = dy - mcy;
        const int32_t d2 = ex * ex + ey * ey;
        const int32_t rOut = (ringR + 2) * (ringR + 2), rIn = (ringR - 1) * (ringR - 1);
        if (d2 <= rOut && d2 >= rIn) c = gold;
        else if (d2 <= 9) c = gold;
        else if (d2 < rIn) c = indigoDeep;
        else if (((sx + dy) % pitch) == 0 || ((sx - dy + 4 * pitch) % pitch) == 0) c = lattice;
        else c = indigo;
      }
      row[px] = c;
    }
  }
}
