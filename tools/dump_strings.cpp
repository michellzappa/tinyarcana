// One-shot extractor: link the compiled deck tables and write their text out
// as the JSON that scripts/build_strings.py compiles into language packs.
// This runs once, when the English text moves out of C and into assets/lang.
// It is kept in the tree so a later deck added in C can be lifted the same way.
//
//   c++ -std=c++17 -O2 -I src -I tools/host_shim tools/dump_strings.cpp \
//       src/deck.cpp src/deck_content.cpp -o /tmp/dump_strings
//   /tmp/dump_strings assets/lang/en
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>

#include "deck.h"

static std::string esc(const char *s) {
  std::string o;
  for (; *s; s++) {
    switch (*s) {
      case '"': o += "\\\""; break;
      case '\\': o += "\\\\"; break;
      case '\n': o += "\\n"; break;
      case '\t': o += "\\t"; break;
      default: o += *s;
    }
  }
  return o;
}

static void arr(FILE *f, const char *key, const char *const *v, int n, bool last) {
  fprintf(f, "  \"%s\": [\n", key);
  for (int i = 0; i < n; i++)
    fprintf(f, "    \"%s\"%s\n", esc(v[i]).c_str(), i + 1 < n ? "," : "");
  fprintf(f, "  ]%s\n", last ? "" : ",");
}

int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : ".";
  mkdir(dir, 0755);
  for (uint8_t d = 0; d < DECK_COUNT; d++) {
    const DeckDefinition &deck = DECKS[d];
    char path[512];
    snprintf(path, sizeof path, "%s/%s.json", dir, deck.id);
    FILE *f = fopen(path, "w");
    if (!f) { perror(path); return 1; }
    fprintf(f, "{\n  \"deck\": \"%s\",\n  \"name\": \"%s\",\n", deck.id, esc(deck.name).c_str());

    fprintf(f, "  \"cards\": [\n");
    for (uint8_t i = 0; i < deck.cardCount; i++) {
      const CardInfo &c = deck.cards[i];
      fprintf(f,
              "    {\"name\": \"%s\", \"ruler\": \"%s\", \"keywords\": \"%s\",\n"
              "     \"essence\": \"%s\",\n"
              "     \"past\": \"%s\",\n"
              "     \"present\": \"%s\",\n"
              "     \"future\": \"%s\",\n"
              "     \"question\": \"%s\"}%s\n",
              esc(c.name).c_str(), esc(c.ruler).c_str(), esc(c.keywords).c_str(),
              esc(c.essence).c_str(), esc(c.past).c_str(), esc(c.present).c_str(),
              esc(c.future).c_str(), esc(c.question).c_str(),
              i + 1 < deck.cardCount ? "," : "");
    }
    fprintf(f, "  ],\n");

    fprintf(f, "  \"pairs\": [\n");
    for (uint8_t i = 0; i < deck.pairCount; i++)
      fprintf(f, "    \"%s\"%s\n", esc(deck.pairs[i].text).c_str(),
              i + 1 < deck.pairCount ? "," : "");
    fprintf(f, "  ],\n");

    const DeckReadingStyle &s = *deck.reading;
    arr(f, "rowNames", s.rowNames, 4, false);
    arr(f, "rowGlosses", s.rowGlosses, 4, false);
    arr(f, "arc", s.arcTemplates, 4, false);
    arr(f, "plane", s.planeTemplates, 4, false);
    arr(f, "dominant", s.dominantTemplates, 5, false);
    arr(f, "transition", s.transitionTemplates, 4, false);
    fprintf(f, "  \"hidden\": \"%s\",\n", esc(s.hiddenTemplate).c_str());
    fprintf(f, "  \"hiddenRepeated\": \"%s\"\n}\n", esc(s.hiddenRepeatedTemplate).c_str());
    fclose(f);
    printf("%s: %u cards, %u pairs -> %s\n", deck.id, deck.cardCount, deck.pairCount, path);
  }
  return 0;
}
