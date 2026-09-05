// Enumerates every spread the device can deal and dumps the reading the
// firmware would compose for it, as JSON Lines on stdout.
//
// The engine is deterministic, so this is the complete output space of
// src/tarot_engine.cpp: 22 * 21 * 20 = 9240 ordered spreads (the README's
// 1540 counts unordered sets of three; Past/Present/Future order changes the
// reading, so the ordered count is the one that matters here).
//
//   c++ -std=c++17 -O2 -I src -I tools/host_shim tools/dump_readings.cpp \
//       src/tarot_engine.cpp src/deck.cpp src/strings.cpp -o /tmp/dump_readings
//   /tmp/dump_readings [deck id] [language code]
//
// It reads the same language packs the firmware does, from
// data/amoled-175-round, so run it from the repository root after
// scripts/build_strings.py.
//
// Buffer size matches innerText in src/main.cpp. If the engine ever composes
// more than that the firmware truncates too, so this reports overruns rather
// than growing quietly.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tarot_data.h"
#include "deck.h"
#include "tarot_engine.h"

static const size_t BUF = 2600;  // == sizeof innerText in main.cpp

static void jsonEscape(const char *s) {
  for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
    switch (*p) {
      case '"':  fputs("\\\"", stdout); break;
      case '\\': fputs("\\\\", stdout); break;
      case '\n': fputs("\\n", stdout);  break;
      case '\r': break;
      case '\t': fputs("\\t", stdout);  break;
      default:
        if (*p < 0x20) printf("\\u%04x", *p);
        else putchar(*p);
    }
  }
}

static const char *LANG_ROOT = "data/amoled-175-round";

int main(int argc, char **argv) {
  const uint8_t deckId = argc > 1 ? (uint8_t)atoi(argv[1]) : 0;
  const char *lang = argc > 2 ? argv[2] : "en";
  const DeckDefinition &deck = deckById(deckId);

  // The host loads packs by relative path; the firmware mounts them at the
  // filesystem root. Point the loader at the build output either way.
  char uiPath[256];
  langPath(uiPath, sizeof uiPath, LANG_ROOT, lang, "ui");
  StringPack ui;
  if (!packLoad(ui, uiPath)) {
    fprintf(stderr, "no %s (run scripts/build_strings.py)\n", uiPath);
    return 2;
  }
  DeckText text;
  if (!deckTextLoad(text, deck, lang, LANG_ROOT)) {
    fprintf(stderr, "no %s pack for %s\n", deck.id, lang);
    return 2;
  }
  char buf[BUF];
  long spreads = 0, truncated = 0;
  size_t longest = 0;

  for (int a = 0; a < MAJOR_COUNT; ++a)
    for (int b = 0; b < MAJOR_COUNT; ++b) {
      if (b == a) continue;
      for (int c = 0; c < MAJOR_COUNT; ++c) {
        if (c == a || c == b) continue;

        Reading r{{(uint8_t)a, (uint8_t)b, (uint8_t)c}};
        const uint8_t hidden = tarotHiddenCard(deck, r);
        const size_t len = tarotCompose(text, ui, r, buf, sizeof buf);

        if (len > longest) longest = len;
        if (len >= sizeof buf - 1) ++truncated;

        printf("{\"past\":%d,\"present\":%d,\"future\":%d,", a, b, c);
        printf("\"past_name\":\"%s\",\"present_name\":\"%s\",\"future_name\":\"%s\",",
               deckCard(text, a).name, deckCard(text, b).name,
               deckCard(text, c).name);
        printf("\"hidden\":%d,\"hidden_name\":\"%s\",", hidden,
               deckCard(text, hidden).name);
        printf("\"elements\":[\"%s\",\"%s\",\"%s\"],",
               packGet(ui, (uint16_t)(ENG_EL_FIRE + deckCard(text, a).element)),
               packGet(ui, (uint16_t)(ENG_EL_FIRE + deckCard(text, b).element)),
               packGet(ui, (uint16_t)(ENG_EL_FIRE + deckCard(text, c).element)));
        printf("\"len\":%zu,\"text\":\"", len);
        jsonEscape(buf);
        printf("\"}\n");
        ++spreads;
      }
    }

  fprintf(stderr, "spreads: %ld\nlongest: %zu bytes (buffer %zu)\ntruncated: %ld\n",
          spreads, longest, BUF, truncated);
  return truncated ? 1 : 0;
}
