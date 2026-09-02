// Sound. A small additive synth rendered on core 0 into the ES8311 over
// I2S. No samples on flash: every sound is a handful of sine partials and
// filtered noise with envelopes, so the whole design lives in audio.cpp.
//
// The codec takes its master clock from the bit clock, so no MCLK pin is
// driven. That pin is the one that differs between the 1.75 and 1.75C
// boards; BCLK, WS, DOUT and the amplifier enable are the same on both and
// on the 1.8.
#pragma once

#include <Arduino.h>

enum Sound : uint8_t {
  SND_BOOT = 0,     // two-note bell as the ring draws
  SND_CUT,          // the deck splits: a breath of noise
  SND_DEAL,         // one soft tick per card leaving the deck
  SND_FLIP,         // a card turns: low tock, bright blip
  SND_OPEN,         // a card fills the screen
  SND_CHORD,        // all three are up
  SND_PAGE,         // next page / next card
  SND_BACK,         // back to the spread
};

bool audioBegin();
bool audioReady();
void audioPlay(Sound s);
// The shuffle drone. Call every frame while the deck is held: level 0..1
// fades it in and out, progress 0..1 raises its pitch and brightness. It
// fades out on its own when level drops to 0.
void audioDrone(float level, float progress);
void audioSetVolume(uint8_t percent);
