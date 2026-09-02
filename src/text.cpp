#include "text.h"

#include <string.h>

#include "board_display.h"
#include "fonts/lora_body.h"
#include "fonts/lora_head.h"
#include "fonts/lora_italic.h"
#include "fonts/lora_small.h"
#include "fonts/lora_title.h"

uint16_t blend565(uint16_t bg, uint16_t fg, uint8_t a) {
  if (a == 0) return bg;
  if (a == 255) return fg;
  const uint32_t ia = 255 - a;
  const uint32_t r = (((bg >> 11) & 0x1F) * ia + ((fg >> 11) & 0x1F) * a) / 255;
  const uint32_t g = (((bg >> 5) & 0x3F) * ia + ((fg >> 5) & 0x3F) * a) / 255;
  const uint32_t b = ((bg & 0x1F) * ia + (fg & 0x1F) * a) / 255;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

static const AaGlyph *glyphFor(const AaFont &f, char c) {
  uint8_t u = (uint8_t)c;
  if (u < f.first || u > f.last) u = '?';
  return &f.glyphs[u - f.first];
}

int16_t txtWidth(const AaFont &f, const char *s, int16_t n, int8_t tracking) {
  if (!s) return 0;
  if (n < 0) n = (int16_t)strlen(s);
  int32_t w = 0;
  for (int16_t i = 0; i < n; i++) w += glyphFor(f, s[i])->adv + tracking;
  if (n > 0) w -= tracking;
  return (int16_t)w;
}

static void drawGlyph(const AaFont &f, const AaGlyph *g, int16_t x,
                      int16_t baseline, uint16_t col) {
  if (g->w == 0 || g->h == 0) return;
  uint16_t *fb = gfx->fb();
  if (!fb) return;
  const int16_t x0 = (int16_t)(x + g->xo);
  const int16_t y0 = (int16_t)(baseline + g->yo);
  const uint8_t *a = f.alpha + g->off;
  for (int16_t j = 0; j < g->h; j++) {
    const int16_t y = (int16_t)(y0 + j);
    if (y < 0 || y >= SCR_H) { a += g->w; continue; }
    uint16_t *row = fb + (int32_t)y * SCR_W;
    for (int16_t i = 0; i < g->w; i++, a++) {
      const int16_t px = (int16_t)(x0 + i);
      if (px < 0 || px >= SCR_W || *a == 0) continue;
      row[px] = blend565(row[px], col, *a);
    }
  }
}

void txtDraw(const AaFont &f, const char *s, int16_t x, int16_t baseline,
             uint16_t col, int16_t n, int8_t tracking) {
  if (!s) return;
  if (n < 0) n = (int16_t)strlen(s);
  for (int16_t i = 0; i < n; i++) {
    const AaGlyph *g = glyphFor(f, s[i]);
    drawGlyph(f, g, x, baseline, col);
    x = (int16_t)(x + g->adv + tracking);
  }
}

void txtCenter(const AaFont &f, const char *s, int16_t cx, int16_t baseline,
               uint16_t col, int8_t tracking) {
  const int16_t w = txtWidth(f, s, -1, tracking);
  txtDraw(f, s, (int16_t)(cx - w / 2), baseline, col, -1, tracking);
}

void txtRight(const AaFont &f, const char *s, int16_t rightX, int16_t baseline,
              uint16_t col, int8_t tracking) {
  const int16_t w = txtWidth(f, s, -1, tracking);
  txtDraw(f, s, (int16_t)(rightX - w), baseline, col, -1, tracking);
}

uint16_t txtFitLine(const AaFont &f, const char *s, int16_t maxW, const char **next) {
  const char *lineStart = s;
  const char *lastBreak = nullptr;
  const char *q = s;
  while (*q) {
    const char *wordEnd = q;
    while (*wordEnd && *wordEnd != ' ') wordEnd++;
    if (txtWidth(f, lineStart, (int16_t)(wordEnd - lineStart)) > maxW && lastBreak) break;
    lastBreak = wordEnd;
    q = wordEnd;
    while (*q == ' ') q++;
    if (!*wordEnd) break;
  }
  const char *lineEnd = lastBreak ? lastBreak : q;
  if (lineEnd == lineStart) {   // one oversized word: hard cut at its end
    while (*lineEnd && *lineEnd != ' ') lineEnd++;
  }
  const char *n = lineEnd;
  while (*n == ' ') n++;
  *next = n;
  return (uint16_t)(lineEnd - lineStart);
}

int16_t txtWrapped(const AaFont &f, const char *s, int16_t x, int16_t baseline,
                   int16_t maxW, int16_t lineH, uint16_t col,
                   uint8_t maxLines, bool center) {
  const char *p = s;
  while (*p == ' ') p++;
  for (uint8_t i = 0; *p && i < maxLines; i++) {
    const char *next;
    const uint16_t len = txtFitLine(f, p, maxW, &next);
    int16_t lx = x;
    if (center) lx = (int16_t)(x + (maxW - txtWidth(f, p, (int16_t)len)) / 2);
    txtDraw(f, p, lx, baseline, col, (int16_t)len);
    baseline = (int16_t)(baseline + lineH);
    p = next;
  }
  return baseline;
}

int16_t txtWrappedFn(const AaFont &f, const char *s, TxtWidthFn widthAt,
                     int16_t baseline, int16_t lineH, uint16_t col,
                     uint8_t maxLines) {
  const char *p = s;
  while (*p == ' ') p++;
  for (uint8_t i = 0; *p && i < maxLines; i++) {
    int16_t x = 0;
    const int16_t w = widthAt(baseline, &x);
    const char *next;
    const uint16_t len = txtFitLine(f, p, w, &next);
    const int16_t lx = (int16_t)(x + (w - txtWidth(f, p, (int16_t)len)) / 2);
    txtDraw(f, p, lx, baseline, col, (int16_t)len);
    baseline = (int16_t)(baseline + lineH);
    p = next;
  }
  return baseline;
}

static const int16_t LAYOUT_LINE_H = 21;
static const int16_t LAYOUT_GAP_H = 8;
static const int16_t LAYOUT_HEAD_H = 24;

uint16_t txtLayout(char *text, TxtWidthFn widthAt, int16_t yTop, int16_t yBottom,
                   TxtLine *out, uint16_t maxLines, uint8_t *pages) {
  uint16_t count = 0;
  uint8_t page = 0;
  int16_t y = yTop;
  char *p = text;

  while (*p && count < maxLines) {
    char *end = strchr(p, '\n');
    if (end) *end = 0;
    const char *para = p;
    p = end ? end + 1 : p + strlen(p);

    uint8_t style = TXT_BODY;
    const char *body = para;
    if (!*para) style = TXT_GAP;
    else if (*para == '#') { style = TXT_HEAD; body = para + 1; }
    else if (*para == '>') { style = TXT_ITALIC; body = para + 1; }
    while (*body == ' ') body++;

    if (style == TXT_GAP) {
      if (y != yTop) y = (int16_t)(y + LAYOUT_GAP_H);
      continue;
    }
    const AaFont &f = style == TXT_HEAD ? lora_small
                      : style == TXT_ITALIC ? lora_italic : lora_body;
    const int16_t lineH = style == TXT_HEAD ? LAYOUT_HEAD_H : LAYOUT_LINE_H;
    // Keep a heading on the same page as at least one line of its body.
    if (y > yBottom || (style == TXT_HEAD && y + LAYOUT_LINE_H > yBottom)) {
      page++;
      y = yTop;
    }
    const char *q = body;
    while (*q && count < maxLines) {
      if (y > yBottom) {
        page++;
        y = yTop;
      }
      int16_t x = 0;
      const int16_t w = widthAt(y, &x);
      const char *next;
      const uint16_t len = txtFitLine(f, q, w, &next);
      const int8_t tracking = style == TXT_HEAD ? 2 : 0;
      out[count].s = q;
      out[count].len = len;
      out[count].style = style;
      out[count].page = page;
      out[count].x = (int16_t)(x + (w - txtWidth(f, q, (int16_t)len, tracking)) / 2);
      out[count].y = (int16_t)(y + (style == TXT_HEAD ? 4 : 0));
      count++;
      y = (int16_t)(y + lineH);
      q = next;
    }
  }
  *pages = (uint8_t)(page + 1);
  return count;
}
