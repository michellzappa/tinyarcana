// Text loaded from the language packs on the filesystem.
//
// Every user-facing string lives in data/<env>/lang/<code>/*.pack, built from
// assets/lang/<code>/*.json by scripts/build_strings.py. Adding a language is
// those JSON files plus `pio run -t uploadfs`; it is never a firmware build.
//
// The engine and the host tools use this header too, so nothing in it may
// depend on Arduino. Only the file read in strings.cpp is board-specific.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "string_ids.h"

struct StringPack {
  const char **s;   // count entries, pointing into blob
  char *blob;
  uint16_t count;
};

bool packLoad(StringPack &p, const char *path);
void packFree(StringPack &p);

// Where a pack lives. The firmware mounts the filesystem at the root, so its
// `root` is "" and the path starts at /lang. The host tools pass the build
// directory instead and read exactly the same files.
void langPath(char *out, size_t n, const char *root, const char *lang,
              const char *file);

// Fills %1 to %4 in a pack string. Never printf: a template is written by a
// translator, and printf would read an argument that is not there the moment
// one of them types %d. %% is a literal percent; anything else passes through.
// Always NUL-terminates, and truncates rather than overruns.
size_t strFormat(char *out, size_t n, const char *fmt, const char *a = "",
                 const char *b = "", const char *c = "", const char *d = "");

// Out-of-range returns "" rather than reading past the table: a pack that is
// one string short must show a gap, not crash the reading it is inside.
static inline const char *packGet(const StringPack &p, uint16_t i) {
  return i < p.count && p.s[i] ? p.s[i] : "";
}

// The UI and engine strings for the selected language. One language is live at
// a time, the way one deck is, so this is a global rather than something
// threaded through every screen.
extern StringPack uiStrings;

static inline const char *T(uint16_t id) { return packGet(uiStrings, id); }
