#include "strings.h"

#include <stdlib.h>
#include <string.h>

#include <stdio.h>

#ifdef ARDUINO
#include <Arduino.h>
#include <LittleFS.h>
#endif

// The one board-specific part: read the whole file. The packs are tens of KB,
// so a single allocation is cheaper than streaming and keeps every string
// contiguous. PSRAM on the device; plain malloc on the host tools.
static char *readAll(const char *path, size_t *size) {
#ifdef ARDUINO
  File f = LittleFS.open(path, "r");
  if (!f) return nullptr;
  const size_t n = f.size();
  char *buf = (char *)ps_malloc(n);
  if (!buf) { f.close(); return nullptr; }
  const size_t got = f.read((uint8_t *)buf, n);
  f.close();
  if (got != n) { free(buf); return nullptr; }
  *size = n;
  return buf;
#else
  FILE *f = fopen(path, "rb");
  if (!f) return nullptr;
  fseek(f, 0, SEEK_END);
  const long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (n <= 0) { fclose(f); return nullptr; }
  char *buf = (char *)malloc((size_t)n);
  if (!buf) { fclose(f); return nullptr; }
  const size_t got = fread(buf, 1, (size_t)n, f);
  fclose(f);
  if (got != (size_t)n) { free(buf); return nullptr; }
  *size = (size_t)n;
  return buf;
#endif
}

void langPath(char *out, size_t n, const char *root, const char *lang,
              const char *file) {
  snprintf(out, n, "%s/lang/%s/%s.pack", root ? root : "", lang, file);
}

size_t strFormat(char *out, size_t n, const char *fmt, const char *a,
                 const char *b, const char *c, const char *d) {
  const char *const arg[4] = {a, b, c, d};
  size_t t = 0;
  if (!n) return 0;
  for (const char *p = fmt; *p && t + 1 < n; p++) {
    if (*p != '%') { out[t++] = *p; continue; }
    if (p[1] == '%') { out[t++] = '%'; p++; continue; }
    if (p[1] >= '1' && p[1] <= '4') {
      const char *v = arg[p[1] - '1'];
      const size_t l = strlen(v);
      if (t + l + 1 >= n) break;
      memcpy(out + t, v, l);
      t += l;
      p++;
      continue;
    }
    out[t++] = '%';
  }
  out[t] = 0;
  return t;
}

static uint16_t rd16(const char *p) {
  return (uint16_t)((uint8_t)p[0] | ((uint8_t)p[1] << 8));
}

static uint32_t rd32(const char *p) {
  return (uint32_t)((uint8_t)p[0] | ((uint8_t)p[1] << 8) | ((uint8_t)p[2] << 16) |
                    ((uint32_t)(uint8_t)p[3] << 24));
}

// Format: "TAP1", uint16 count, uint16 reserved, uint32 offsets[count + 1],
// then the NUL-terminated strings. Every field is checked before it is used:
// a truncated or foreign file must fail the load, not produce pointers into
// whatever follows it in memory.
bool packLoad(StringPack &p, const char *path) {
  p.s = nullptr;
  p.blob = nullptr;
  p.count = 0;

  size_t size = 0;
  char *raw = readAll(path, &size);
  if (!raw) return false;
  if (size < 8 || memcmp(raw, "TAP1", 4) != 0) { free(raw); return false; }

  const uint16_t count = rd16(raw + 4);
  const size_t table = 8 + ((size_t)count + 1) * 4;
  if (count == 0 || size < table) { free(raw); return false; }

  const char *const blob = raw + table;
  const size_t blobLen = size - table;
  if (rd32(raw + table - 4) != blobLen) { free(raw); return false; }

  const char **s = (const char **)malloc(count * sizeof(const char *));
  if (!s) { free(raw); return false; }
  for (uint16_t i = 0; i < count; i++) {
    const uint32_t off = rd32(raw + 8 + (size_t)i * 4);
    if (off >= blobLen) { free(s); free(raw); return false; }
    s[i] = blob + off;
  }
  // The last byte must terminate the last string, so every pointer above is a
  // valid C string without any further bounds checking at read time.
  if (blob[blobLen - 1] != 0) { free(s); free(raw); return false; }

  p.s = s;
  p.blob = raw;
  p.count = count;
  return true;
}

void packFree(StringPack &p) {
  free(p.s);
  free(p.blob);
  p.s = nullptr;
  p.blob = nullptr;
  p.count = 0;
}
