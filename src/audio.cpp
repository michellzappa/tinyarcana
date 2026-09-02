#include "audio.h"

#include <ESP_I2S.h>
#include <Wire.h>
#include <math.h>
#include <string.h>

#include "es8311.h"
#include "pin_config.h"

static const int SAMPLE_RATE = 22050;
static const size_t BLOCK = 256;          // frames per render
static const int CODEC_VOLUME = 80;

static I2SClass i2s;
static es8311_handle_t codec = nullptr;
static bool ready = false;
static float master = 0.7f;

// ---------------- Voices ----------------
struct Voice {
  volatile bool on;
  float freq;
  float phase;
  float amp;
  float env;          // current envelope
  float attack;       // per-sample increment while rising
  float decay;        // per-sample multiplier while falling
  bool rising;
  bool noise;         // white noise through a one-pole lowpass
  float lp;           // lowpass state
  float lpCoef;       // 0..1, higher = brighter
  uint32_t startAt;   // millis() at which the voice starts
};

static const uint8_t NV = 12;
static Voice voices[NV];

static uint32_t rngState = 0x1234567;
static inline float noiseSample() {
  rngState ^= rngState << 13;
  rngState ^= rngState >> 17;
  rngState ^= rngState << 5;
  return ((rngState & 0xFFFF) / 32768.0f) - 1.0f;
}

static Voice *freeVoice() {
  for (uint8_t i = 0; i < NV; i++) if (!voices[i].on) return &voices[i];
  // Steal the quietest.
  Voice *q = &voices[0];
  for (uint8_t i = 1; i < NV; i++) if (voices[i].env < q->env) q = &voices[i];
  return q;
}

// attackMs, decayMs: time to rise, and time for the tail to fall to -60 dB.
static void tone(float freq, float amp, float attackMs, float decayMs, uint32_t delayMs = 0,
                 bool noise = false, float lpCoef = 1.0f) {
  Voice *v = freeVoice();
  v->on = false;
  v->freq = freq;
  v->phase = 0;
  v->amp = amp;
  v->env = 0;
  v->rising = true;
  v->attack = attackMs <= 0 ? 1.0f : 1000.0f / (attackMs * SAMPLE_RATE);
  v->decay = powf(0.001f, 1000.0f / (decayMs * SAMPLE_RATE));
  v->noise = noise;
  v->lp = 0;
  v->lpCoef = lpCoef;
  v->startAt = millis() + delayMs;
  v->on = true;
}

// ---------------- Drone ----------------
// Two detuned low sines, a fifth above, and a slow shimmer. Level and
// progress are set from the UI thread and smoothed here.
static volatile float droneTarget = 0, droneProgress = 0;
static float droneLevel = 0;
static float dph[4] = {0, 0, 0, 0};
static float lfo = 0;

static float renderDrone() {
  droneLevel += (droneTarget - droneLevel) * 0.0004f;
  if (droneLevel < 0.0005f) return 0;
  const float p = droneProgress;
  const float base = 82.4f * (1.0f + 0.5f * p);          // E2 rising a fifth
  const float f[4] = {base, base * 1.006f, base * 1.5f, base * 3.0f * (1.0f + 0.002f * p)};
  const float a[4] = {0.9f, 0.7f, 0.45f, 0.10f + 0.35f * p};
  lfo += 2.0f * 3.14159265f * (0.35f + 1.2f * p) / SAMPLE_RATE;
  const float shimmer = 0.75f + 0.25f * sinf(lfo);
  float s = 0;
  for (uint8_t i = 0; i < 4; i++) {
    dph[i] += 2.0f * 3.14159265f * f[i] / SAMPLE_RATE;
    if (dph[i] > 6.2831853f) dph[i] -= 6.2831853f;
    s += sinf(dph[i]) * a[i];
  }
  return s * 0.16f * droneLevel * shimmer;
}

// ---------------- Render task ----------------
static int16_t block[BLOCK * 2];

static void renderBlock() {
  const uint32_t now = millis();
  for (size_t n = 0; n < BLOCK; n++) {
    float mix = renderDrone();
    for (uint8_t i = 0; i < NV; i++) {
      Voice &v = voices[i];
      if (!v.on || now < v.startAt) continue;
      if (v.rising) {
        v.env += v.attack;
        if (v.env >= 1.0f) { v.env = 1.0f; v.rising = false; }
      } else {
        v.env *= v.decay;
        if (v.env < 0.0005f) { v.on = false; continue; }
      }
      float s;
      if (v.noise) {
        v.lp += (noiseSample() - v.lp) * v.lpCoef;
        s = v.lp;
      } else {
        v.phase += 2.0f * 3.14159265f * v.freq / SAMPLE_RATE;
        if (v.phase > 6.2831853f) v.phase -= 6.2831853f;
        s = sinf(v.phase);
      }
      mix += s * v.amp * v.env;
    }
    mix *= master;
    // Soft clip.
    if (mix > 1.0f) mix = 1.0f; else if (mix < -1.0f) mix = -1.0f;
    mix = mix * (1.5f - 0.5f * mix * mix);
    const int16_t out = (int16_t)(mix * 32000.0f);
    block[n * 2] = out;
    block[n * 2 + 1] = out;
  }
}

static void audioTask(void *) {
  for (;;) {
    renderBlock();
    i2s.write((const uint8_t *)block, sizeof block);   // blocks on DMA
  }
}

// ---------------- Codec ----------------
static bool codecBegin() {
  codec = es8311_create(0 /* Wire */, ES8311_I2C_ADDR);
  if (!codec) { Serial.println("audio: ES8311 create failed"); return false; }
  const es8311_clock_config_t clk = {
      .mclk_inverted = false,
      .sclk_inverted = false,
      .mclk_from_mclk_pin = false,   // BCLK feeds the codec's clock tree
      .mclk_frequency = 0,
      .sample_frequency = SAMPLE_RATE,
  };
  if (es8311_init(codec, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) != ESP_OK) {
    Serial.println("audio: ES8311 init failed");
    return false;
  }
  es8311_microphone_config(codec, false);
  es8311_voice_volume_set(codec, CODEC_VOLUME, nullptr);
  es8311_voice_mute(codec, false);
  return true;
}

bool audioBegin() {
  pinMode(PA_EN, OUTPUT);
  digitalWrite(PA_EN, HIGH);
  i2s.setPins(I2S_BCK_IO, I2S_WS_IO, I2S_DO_IO, -1 /* no capture */, -1 /* no MCLK */);
  if (!i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT,
                 I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH)) {
    Serial.println("audio: I2S begin failed");
    return false;
  }
  if (!codecBegin()) return false;
  memset(voices, 0, sizeof voices);
  ready = true;
  xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, 3, nullptr, 0);
  Serial.printf("audio: ES8311 @ %d Hz, BCLK-clocked, PA on GPIO%d\n", SAMPLE_RATE, PA_EN);
  return true;
}

bool audioReady() { return ready; }

void audioSetVolume(uint8_t percent) {
  master = 0.7f * percent / 100.0f;
}

void audioDrone(float level, float progress) {
  if (!ready) return;
  if (level < 0) level = 0;
  if (level > 1) level = 1;
  droneTarget = level;
  droneProgress = progress < 0 ? 0 : progress > 1 ? 1 : progress;
}

// ---------------- Sound design ----------------
void audioPlay(Sound s) {
  if (!ready) return;
  switch (s) {
  case SND_BOOT:
    // A struck bell: fundamental, a slightly sharp octave, a twelfth; then
    // a second, higher note as the title fades in.
    tone(523.25f, 0.30f, 4, 1400);
    tone(1046.5f * 1.004f, 0.12f, 4, 900);
    tone(1568.0f, 0.06f, 4, 500);
    tone(659.25f, 0.26f, 4, 1600, 420);
    tone(1318.5f * 1.004f, 0.10f, 4, 1000, 420);
    tone(1975.5f, 0.05f, 4, 600, 420);
    break;
  case SND_CUT:
    // Breath: low-passed noise that opens as it swells, plus a soft thud.
    tone(0, 0.35f, 90, 260, 0, true, 0.08f);
    tone(0, 0.18f, 200, 180, 120, true, 0.30f);
    tone(98.0f, 0.30f, 2, 160, 380);
    break;
  case SND_DEAL:
    tone(880.0f, 0.10f, 1, 60);
    tone(1760.0f, 0.04f, 1, 40);
    break;
  case SND_FLIP:
    tone(196.0f, 0.28f, 1, 110);
    tone(0, 0.10f, 1, 40, 0, true, 0.6f);
    tone(1567.98f, 0.10f, 1, 180, 90);
    tone(2349.3f, 0.05f, 1, 120, 90);
    break;
  case SND_OPEN:
    // Rising pair: the card arrives.
    tone(392.0f, 0.16f, 3, 400);
    tone(587.33f, 0.16f, 3, 500, 110);
    tone(1174.66f, 0.05f, 3, 400, 110);
    break;
  case SND_CHORD:
    // C E G, staggered, ringing.
    tone(261.63f, 0.22f, 6, 1600);
    tone(329.63f, 0.20f, 6, 1600, 140);
    tone(392.00f, 0.20f, 6, 1600, 280);
    tone(1046.5f * 1.003f, 0.06f, 6, 1200, 280);
    break;
  case SND_PAGE:
    tone(1318.5f, 0.09f, 1, 70);
    break;
  case SND_BACK:
    tone(587.33f, 0.14f, 2, 160);
    tone(392.00f, 0.14f, 2, 220, 80);
    break;
  }
}
