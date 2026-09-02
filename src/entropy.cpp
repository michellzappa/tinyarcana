#include "entropy.h"

#include <bootloader_random.h>
#include <esp_random.h>

static uint64_t state = 0;
static uint32_t stirs = 0;

static uint64_t splitmix64(uint64_t &x) {
  uint64_t z = (x += 0x9E3779B97F4A7C15ULL);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
  return z ^ (z >> 31);
}

void entropyBegin() {
  // No ADC, I2S or radio in this firmware, so the SAR ADC entropy source can
  // stay enabled for the whole run.
  bootloader_random_enable();
  state = ((uint64_t)esp_random() << 32) | esp_random();
  state ^= (uint64_t)micros();
  stirs = 0;
}

void entropyStir(uint32_t a, uint32_t b) {
  state ^= ((uint64_t)a << 32) | b;
  state ^= (uint64_t)esp_random() << 17;
  state ^= (uint64_t)micros();
  (void)splitmix64(state);
  stirs++;
}

uint32_t entropyStirs() { return stirs; }

uint32_t entropyNext() {
  // Hardware word XOR stirred stream: neither source alone decides the card.
  return esp_random() ^ (uint32_t)(splitmix64(state) >> 32);
}

void entropyDraw(uint8_t *out, uint8_t n, uint8_t range) {
  uint8_t got = 0;
  const uint32_t limit = 0xFFFFFFFFu - (0xFFFFFFFFu % range);   // no modulo bias
  while (got < n) {
    uint32_t r = entropyNext();
    if (r >= limit) continue;
    const uint8_t v = (uint8_t)(r % range);
    bool dup = false;
    for (uint8_t i = 0; i < got; i++) if (out[i] == v) dup = true;
    if (!dup) out[got++] = v;
  }
}
