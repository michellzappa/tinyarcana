// Randomness for the draw.
//
// The ESP32-S3 hardware RNG samples thermal and RF noise; with the radio off
// it is fed by the SAR ADC entropy source that bootloader_random_enable()
// turns on. That is physical noise, not a PRNG. On top of it the shuffle
// stirs in the user's touch: position, timing and hold length. The draw is
// unpredictable to the firmware author as well as the user.
#pragma once

#include <Arduino.h>

void entropyBegin();
void entropyStir(uint32_t a, uint32_t b);
uint32_t entropyStirs();
uint32_t entropyNext();
// n unique values in [0, range). Reservoir of hardware + stirred state.
void entropyDraw(uint8_t *out, uint8_t n, uint8_t range);
