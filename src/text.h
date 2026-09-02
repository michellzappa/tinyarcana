// Anti-aliased text straight into the PSRAM framebuffer.
#pragma once

#include <Arduino.h>

#include "aafont.h"

extern const AaFont lora_title;    // 26px, names
extern const AaFont lora_head;     // 18px, screen headers
extern const AaFont lora_body;     // 15px, body text
extern const AaFont lora_italic;   // 15px, questions and asides
extern const AaFont lora_read;     // 17px, the readings
extern const AaFont lora_read_italic;
extern const AaFont lora_small;    // 12px, labels (use uppercase + tracking)

uint16_t blend565(uint16_t bg, uint16_t fg, uint8_t a);

int16_t txtWidth(const AaFont &f, const char *s, int16_t n = -1, int8_t tracking = 0);
void txtDraw(const AaFont &f, const char *s, int16_t x, int16_t baseline,
             uint16_t col, int16_t n = -1, int8_t tracking = 0);
void txtCenter(const AaFont &f, const char *s, int16_t cx, int16_t baseline,
               uint16_t col, int8_t tracking = 0);
void txtRight(const AaFont &f, const char *s, int16_t rightX, int16_t baseline,
              uint16_t col, int8_t tracking = 0);

// How wide a line at this baseline may be, and where it starts. A round
// face answers with the chord; a rectangle with a constant.
typedef int16_t (*TxtWidthFn)(int16_t baseline, int16_t *xLeft);

// Longest prefix of s (breaking at spaces) that fits in maxW. Returns the
// length; *next is where the following line starts.
uint16_t txtFitLine(const AaFont &f, const char *s, int16_t maxW, const char **next);

// Wrap and draw one paragraph at a fixed width. Returns the next baseline.
int16_t txtWrapped(const AaFont &f, const char *s, int16_t x, int16_t baseline,
                   int16_t maxW, int16_t lineH, uint16_t col,
                   uint8_t maxLines, bool center = false);
// Same, with the width asked per line. Lines are centered in that width.
int16_t txtWrappedFn(const AaFont &f, const char *s, TxtWidthFn widthAt,
                     int16_t baseline, int16_t lineH, uint16_t col,
                     uint8_t maxLines);

// Multi-paragraph, paged layout. Paragraphs are separated by '\n'.
// A paragraph starting with '#' is a heading, '>' is italic, empty is a gap.
enum TxtStyle : uint8_t { TXT_BODY = 0, TXT_ITALIC, TXT_HEAD, TXT_GAP };

struct TxtLine {
  const char *s;
  uint16_t len;
  uint8_t style;
  uint8_t page;
  int16_t x;
  int16_t y;   // baseline
};

// Mutates text: paragraph separators become NULs so lines can point into it.
// Every line is centered in the width its row allows.
uint16_t txtLayout(char *text, TxtWidthFn widthAt, int16_t yTop, int16_t yBottom,
                   TxtLine *out, uint16_t maxLines, uint8_t *pages,
                   const AaFont &body = lora_body, const AaFont &italic = lora_italic,
                   int16_t lineH = 21);
