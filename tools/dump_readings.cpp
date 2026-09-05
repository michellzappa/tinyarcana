// Enumerates every spread the device can deal and dumps the reading the
// firmware would compose for it, as JSON Lines on stdout.
//
// The engine is deterministic, so this is the complete output space of
// src/tarot_engine.cpp: 22 * 21 * 20 = 9240 ordered spreads (the README's
// 1540 counts unordered sets of three; Past/Present/Future order changes the
// reading, so the ordered count is the one that matters here).
//
//   c++ -std=c++17 -O2 -I src -I tools/host_shim \
//       tools/dump_readings.cpp src/tarot_engine.cpp -o /tmp/dump_readings
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

int main(int argc, char **argv) {
  const uint8_t deckId = argc > 1 ? (uint8_t)atoi(argv[1]) : 0;
  const DeckDefinition &deck = deckById(deckId);
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
        const size_t len = tarotCompose(deck, r, buf, sizeof buf);

        if (len > longest) longest = len;
        if (len >= sizeof buf - 1) ++truncated;

        printf("{\"past\":%d,\"present\":%d,\"future\":%d,", a, b, c);
        printf("\"past_name\":\"%s\",\"present_name\":\"%s\",\"future_name\":\"%s\",",
               deckCard(deck, a).name, deckCard(deck, b).name,
               deckCard(deck, c).name);
        printf("\"hidden\":%d,\"hidden_name\":\"%s\",", hidden,
               deckCard(deck, hidden).name);
        printf("\"elements\":[\"%s\",\"%s\",\"%s\"],",
               ELEMENT_NAME[deckCard(deck, a).element],
               ELEMENT_NAME[deckCard(deck, b).element],
               ELEMENT_NAME[deckCard(deck, c).element]);
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
