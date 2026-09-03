// Tarot for two Waveshare AMOLEDs (round 1.75 by default, 1.8 portrait).
//
//   boot -> deck -(hold, release)-> cut -> deal -> spread -(tap)-> card -(tap)-> meaning
//                                                   |                            |
//                                                  PWR -------> inner <---------PWR
//   Close a reading from any screen after the deal: press PWR while holding
//   BOOT, or hold BOOT on its own. The cards gather back into the deck and
//   the next shuffle starts from scratch.
#include <Arduino.h>

#include "audio.h"
#include "board_display.h"
#include "board_input.h"
#include "cards.h"
#include "entropy.h"
#include "tarot_data.h"
#include "tarot_engine.h"
#include "ui.h"

enum Screen : uint8_t {
  SCR_BOOT = 0,
  SCR_DECK,
  SCR_HELP,
  SCR_CUT,
  SCR_DEAL,
  SCR_GATHER,    // cards return to the deck
  SCR_SPREAD,
  SCR_FLIP,
  SCR_ZOOM,      // slot -> full card
  SCR_CARD,      // one card, large
  SCR_MEANING,   // its text
  SCR_INNER,
};

static const uint32_t BOOT_MS = 2900;
static const uint32_t SHUFFLE_MS = 2400;
static const uint32_t CUT_MS = 700;
static const uint32_t DEAL_MS = 1100;
static const uint32_t GATHER_MS = 1000;
static const uint32_t FLIP_MS = 420;
static const uint32_t ZOOM_MS = 380;

static Screen screen = SCR_BOOT;
static uint32_t enterMs = 0;
static bool fsOk = false, touchOk = false, audioOk = false;

static Spread spread;
static int8_t flipSlot = -1;
static uint8_t cardPos = 0;
static uint8_t innerPage = 0, innerPages = 1;
static bool innerReady = false;
static char innerText[2600];
static uint8_t dealt = 0;
static bool holding = false;
static uint32_t holdStart = 0;

static void go(Screen s) {
  screen = s;
  enterMs = millis();
}

static void newReading() {
  entropyDraw(spread.reading.card, 3, 22);
  for (uint8_t i = 0; i < 3; i++) spread.revealed[i] = false;
  innerReady = false;
  Serial.printf("draw: %s / %s / %s  (stirs=%lu)\n",
                CARDS[spread.reading.card[0]].name, CARDS[spread.reading.card[1]].name,
                CARDS[spread.reading.card[2]].name, (unsigned long)entropyStirs());
  for (uint8_t i = 0; i < 3; i++) cardPreload(spread.reading.card[i]);
}

static bool allRevealed() {
  return spread.revealed[0] && spread.revealed[1] && spread.revealed[2];
}

static void startFlip(uint8_t slot) {
  flipSlot = (int8_t)slot;
  audioPlay(SND_FLIP);
  go(SCR_FLIP);
}

static void openCard(uint8_t pos, Sound snd) {
  cardPos = pos;
  audioPlay(snd);
  go(SCR_CARD);
}

// From the spread: the card grows out of its slot first.
static void zoomCard(uint8_t pos) {
  cardPos = pos;
  audioPlay(SND_OPEN);
  go(SCR_ZOOM);
}

static void backToSpread() {
  audioPlay(SND_BACK);
  go(SCR_SPREAD);
}

static void openInner() {
  if (!innerReady) {
    tarotCompose(spread.reading, innerText, sizeof innerText);
    innerPages = uiInnerPrepare(innerText);
    innerReady = true;
  }
  innerPage = 0;
  audioPlay(SND_OPEN);
  go(SCR_INNER);
}

static void startCut() {
  newReading();
  audioPlay(SND_CUT);
  go(SCR_CUT);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== tarot ===");
  entropyBegin();
  if (!boardDisplayBegin()) Serial.println("display init failed");
  boardInputBegin();
  touchOk = boardTouchPresent();
  audioOk = audioBegin();
  fsOk = cardsBegin();
  gfx->clear(COL_BG);
  gfx->flush();
  audioPlay(SND_BOOT);
  go(SCR_BOOT);
}

void loop() {
  const uint32_t now = millis();
  const uint32_t age = now - enterMs;
  InputFrame in;
  boardInputPoll(&in);

  // Close the reading: BOOT held + PWR, or BOOT held on its own for 800 ms.
  // After the chord the BOOT hold that is still in progress must not fire
  // its own reset (or count as a press) once we are back on the deck.
  static bool bootChordUsed = false;
  if (!in.aDown) bootChordUsed = false;
  if (screen >= SCR_SPREAD && ((in.bPressed && in.aDown) || in.aLong)) {
    Serial.println(in.aLong ? "btn BOOT long -> gather" : "btn BOOT+PWR -> gather");
    bootChordUsed = true;
    holding = false;
    dealt = 0;
    audioPlay(SND_BACK);
    go(SCR_GATHER);
    return;
  }
  if (bootChordUsed) {
    in.aLong = false;
    in.aPressed = false;
    in.bPressed = false;
  }

  switch (screen) {
  case SCR_BOOT:
    uiBoot(age, fsOk, touchOk);
    if (age > BOOT_MS || (age > 600 && (in.aPressed || in.tap))) go(SCR_DECK);
    break;

  case SCR_DECK: {
    if (in.touchBegan && uiDeckHit(in.x, in.y)) {
      holding = true;
      holdStart = now;
    }
    float progress = 0;
    if (holding && in.touchDown) {
      entropyStir(((uint32_t)in.x << 16) | (uint16_t)in.y, now);
      progress = (now - holdStart) / (float)SHUFFLE_MS;
      if (progress > 1) progress = 1;
      audioDrone(1.0f, progress);
    }
    if (holding && in.touchEnded) {
      holding = false;
      progress = (now - holdStart) / (float)SHUFFLE_MS;
      audioDrone(0, progress);
      if (progress >= 1.0f) {
        entropyStir(now - holdStart, entropyStirs());
        startCut();
      }
      progress = 0;
    }
    uiDeck(now, holding && in.touchDown, progress);
    if (in.bPressed) { audioPlay(SND_PAGE); go(SCR_HELP); }
    // BOOT on the deck: a quick draw for the impatient (hardware noise only).
    if (in.aPressed) startCut();
    break;
  }

  case SCR_HELP:
    uiHelp();
    if (in.tap || in.aPressed || in.bPressed) { audioPlay(SND_BACK); go(SCR_DECK); }
    break;

  case SCR_CUT:
    uiCut(age / (float)CUT_MS);
    if (age >= CUT_MS) { dealt = 0; go(SCR_DEAL); }
    break;

  case SCR_DEAL: {
    const float p = age / (float)DEAL_MS;
    // One tick as each card leaves the deck (uiDeal starts card i at i*0.22).
    while (dealt < 3 && p >= dealt * 0.22f) { audioPlay(SND_DEAL); dealt++; }
    uiDeal(p);
    if (age >= DEAL_MS) go(SCR_SPREAD);
    break;
  }

  case SCR_GATHER: {
    const float p = age / (float)GATHER_MS;
    while (dealt < 3 && p >= dealt * 0.22f) { audioPlay(SND_DEAL); dealt++; }
    uiGather(p);
    if (age >= GATHER_MS) go(SCR_DECK);
    break;
  }

  case SCR_SPREAD: {
    uiSpread(spread, -1, 0);
    if (in.tap) {
      const int8_t slot = uiSlotHit(in.x, in.y);
      if (slot >= 0) {
        if (!spread.revealed[slot]) startFlip((uint8_t)slot);
        else zoomCard((uint8_t)slot);
      }
    }
    if (in.aPressed) {
      // BOOT: turn the next card, or walk into the first card once all are up.
      int8_t next = -1;
      for (uint8_t i = 0; i < 3; i++) if (!spread.revealed[i]) { next = (int8_t)i; break; }
      if (next >= 0) startFlip((uint8_t)next);
      else zoomCard(0);
    }
    if (in.bPressed) {
      if (allRevealed()) openInner();
      else {
        // Turn what is still face down first; PWR again opens the reading.
        for (uint8_t i = 0; i < 3; i++) if (!spread.revealed[i]) { startFlip(i); break; }
      }
    }
    break;
  }

  case SCR_FLIP: {
    const float p = age / (float)FLIP_MS;
    uiSpread(spread, flipSlot, p < 1 ? p : 1);
    if (age >= FLIP_MS) {
      spread.revealed[flipSlot] = true;
      flipSlot = -1;
      if (allRevealed()) audioPlay(SND_CHORD);
      go(SCR_SPREAD);
    }
    break;
  }

  case SCR_ZOOM: {
    const float p = age / (float)ZOOM_MS;
    uiZoom(spread, cardPos, p < 1 ? p : 1);
    if (age >= ZOOM_MS) go(SCR_CARD);
    break;
  }

  case SCR_CARD:
    uiCardBig(spread, cardPos);
    if (in.tap || in.aPressed) { audioPlay(SND_PAGE); go(SCR_MEANING); }
    else if (in.swipeLeft) { if (cardPos < 2) openCard(cardPos + 1, SND_PAGE); else backToSpread(); }
    else if (in.swipeRight) { if (cardPos > 0) openCard(cardPos - 1, SND_PAGE); else backToSpread(); }
    if (in.bPressed) openInner();
    break;

  case SCR_MEANING:
    uiMeaning(spread, cardPos);
    if (in.tap) backToSpread();
    else if (in.aPressed) { if (cardPos < 2) openCard(cardPos + 1, SND_PAGE); else backToSpread(); }
    else if (in.swipeLeft) { if (cardPos < 2) openCard(cardPos + 1, SND_PAGE); else backToSpread(); }
    else if (in.swipeRight) { if (cardPos > 0) openCard(cardPos - 1, SND_PAGE); else backToSpread(); }
    if (in.bPressed) openInner();
    break;

  case SCR_INNER:
    uiInner(spread, innerPage, innerPages);
    if (in.tap || in.swipeLeft || in.aPressed) {
      if (innerPage + 1 < innerPages) { innerPage++; audioPlay(SND_PAGE); }
      else backToSpread();
    } else if (in.swipeRight && innerPage > 0) {
      innerPage--;
      audioPlay(SND_PAGE);
    }
    if (in.bPressed) backToSpread();
    break;
  }

  gfx->flush();
}
