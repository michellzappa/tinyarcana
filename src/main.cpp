// Tarot for the Waveshare ESP32-S3-Touch-AMOLED-1.75 round display.
//
//   boot -> deck -(hold, release)-> cut -> deal -> spread -(tap)-> card -(tap)-> meaning
//                                                   |                            |
//                                                  PWR -------> inner <---------PWR
//   Close a reading from any screen after the deal: press PWR while holding
//   BOOT, or hold BOOT on its own. The cards gather back into the deck and
//   the next shuffle starts from scratch.
#include <Arduino.h>

#include <string.h>

#include "board_display.h"
#include "board_input.h"
#include "cards.h"
#include "deck.h"
#include "entropy.h"
#include "power.h"
#include "settings.h"
#include "tarot_data.h"
#include "tarot_engine.h"
#include "ui.h"

enum Screen : uint8_t {
  SCR_BOOT = 0,
  SCR_DECK,
  SCR_MENU,
  SCR_HELP,
  SCR_SETTINGS,
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
static const uint32_t DEAL_MS = 1100;
static const uint32_t GATHER_MS = 1000;
// A single card is a quick question, so it gets a quick ritual: a short press
// rather than a hold, and one card has no stagger to wait for on the way out.
// The deck screen draws at about four frames a second, so 1000 ms is roughly
// four frames of riffle. Shorter than that and nothing moves at all.
static const uint32_t DEAL_ONE_MS = 300;
static const uint32_t GATHER_ONE_MS = 300;
static const uint32_t FLIP_ONE_MS = 300;
// How long the back sits there before it turns itself over. Long enough to
// register as a card arriving, short enough not to feel like a wait.
static const uint32_t CARD_DWELL_MS = 260;

static const uint32_t FLIP_MS = 420;
static const uint32_t ZOOM_MS = 380;

static Screen screen = SCR_BOOT;
static uint32_t enterMs = 0;
static bool fsOk = false, touchOk = false;

static Spread spread = {{{0, 0, 0}}, 0, {false, false, false}, 3};
static uint32_t dealMs() { return spread.count == 1 ? DEAL_ONE_MS : DEAL_MS; }
static uint32_t gatherMs() { return spread.count == 1 ? GATHER_ONE_MS : GATHER_MS; }
static int8_t flipSlot = -1;
static uint8_t cardPos = 0;
static uint8_t innerPage = 0, innerPages = 1;
static bool innerReady = false;
static char innerText[2600];
static uint8_t menuCursor = 0;
static uint8_t settingsCursor = 0;
static uint8_t dealt = 0;
static bool holding = false;
static uint32_t holdStart = 0;

static void go(Screen s) {
  screen = s;
  enterMs = millis();
}

static void newReading() {
  spread.deck = appSettings.deckId;
  const DeckDefinition &deck = deckById(spread.deck);
  cardsSelectDeck(deck);
  spread.count = appSettings.singleCard ? 1 : 3;
  entropyDraw(spread.reading.card, spread.count, deck.cardCount);
  for (uint8_t i = 0; i < 3; i++) spread.revealed[i] = false;
  innerReady = false;
  if (spread.count == 1) {
    Serial.printf("draw: %s  (stirs=%lu)\n", deckCard(deckText, spread.reading.card[0]).name,
                  (unsigned long)entropyStirs());
  } else {
    Serial.printf("draw: %s / %s / %s  (stirs=%lu)\n",
                  deckCard(deckText, spread.reading.card[0]).name,
                  deckCard(deckText, spread.reading.card[1]).name,
                  deckCard(deckText, spread.reading.card[2]).name,
                  (unsigned long)entropyStirs());
  }
  for (uint8_t i = 0; i < spread.count; i++) cardPreload(spread.reading.card[i]);
}

static bool allRevealed() {
  for (uint8_t i = 0; i < spread.count; i++) if (!spread.revealed[i]) return false;
  return true;
}

static void startFlip(uint8_t slot) {
  flipSlot = (int8_t)slot;
  go(SCR_FLIP);
}

static void openCard(uint8_t pos) {
  cardPos = pos;
  go(SCR_CARD);
}

// From the spread: the card grows out of its slot first.
static void zoomCard(uint8_t pos) {
  cardPos = pos;
  go(SCR_ZOOM);
}

static void backToSpread() {
  // With one card there is nothing behind it, so backing out gathers instead.
  if (spread.count == 1) { dealt = 0; go(SCR_GATHER); }
  else go(SCR_SPREAD);
}

static void openInner() {
  // The composer reads an arc across three positions, the elements between
  // neighbours, curated pairs and a digit-reduced sum. A single card has none
  // of those, so there is no inner reading to open. Guarded here rather than
  // at each caller: PWR reaches this from the spread, the card and the
  // meaning page.
  if (spread.count != 3) return;
  if (!innerReady) {
    tarotCompose(deckText, uiStrings, spread.reading, innerText,
                 sizeof innerText);
    innerPages = uiInnerPrepare(innerText);
    innerReady = true;
  }
  innerPage = 0;
  go(SCR_INNER);
}

// The cut happens on release. It has no screen of its own any more: the deck
// riffles during the hold, so a separate cut animation only delayed the deal.
static void startCut() {
  newReading();
  dealt = 0;
  go(SCR_DEAL);
}

// The reset row asks twice, and the arming is state the screen must show, so
// it lives beside the cursor rather than inside adjustSetting().
static bool resetArmed = false;

// One step of a row's value. The touch screen sets brightness by dragging, but
// BOOT and PWR still have to reach every row, so buttons step it.
static void adjustSetting() {
  if (settingsCursor != SET_ROW_RESET) resetArmed = false;
  switch (settingsCursor) {
  case SET_ROW_DECK:
    appSettings.deckId = (uint8_t)((appSettings.deckId + 1) % DECK_COUNT);
    cardsSelectDeck(deckById(appSettings.deckId));
    // The deck's words and its pictures are the same choice, so they move
    // together; the reading on screen would otherwise name the other deck.
    langApply(appSettings.lang, appSettings.deckId);
    break;
  case SET_ROW_BRIGHT:
    appSettings.brightness =
        (appSettings.brightness >= 224) ? 32
                                        : (uint8_t)(appSettings.brightness + 32);
    break;
  case SET_ROW_LANG: {
    // Step to the next language directory on the filesystem. With only one
    // installed the row is inert, which is the honest thing for it to be.
    const uint8_t n = langCount();
    if (n < 2) break;
    const uint8_t next = (uint8_t)((langIndexOf(appSettings.lang) + 1) % n);
    snprintf(appSettings.lang, sizeof appSettings.lang, "%s", langCodeAt(next));
    langApply(appSettings.lang, appSettings.deckId);
    break;
  }
  case SET_ROW_RESET:
    if (!resetArmed) {
      resetArmed = true;
      return;   // nothing changed yet, so nothing to save
    }
    resetArmed = false;
    settingsReset();
    cardsSelectDeck(deckById(appSettings.deckId));
    langApply(appSettings.lang, appSettings.deckId);
    return;     // settingsReset() already saved and applied
  }
  settingsSave();
  settingsApplyHardware();
}

static void openMenuItem(uint8_t item) {
  if (item == 0) go(SCR_HELP);
  else {
    settingsCursor = 0;
    go(SCR_SETTINGS);
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== tarot ===");
  settingsBegin();
  entropyBegin();
  if (!boardDisplayBegin()) Serial.println("display init failed");
  boardInputBegin();
  touchOk = boardTouchPresent();
  settingsApplyHardware();
  fsOk = cardsBegin();
  if (fsOk) fsOk = cardsSelectDeck(deckById(appSettings.deckId));
  if (!langApply(appSettings.lang, appSettings.deckId))
    Serial.printf("lang: %s unavailable, using en\n", appSettings.lang);
  else
    Serial.printf("lang: %s (%u ui strings)\n", appSettings.lang, uiStrings.count);
  gfx->clear(COL_BG);
  gfx->flush();
  go(SCR_BOOT);
}

void loop() {
  const uint32_t now = millis();
  const uint32_t age = now - enterMs;
  InputFrame in;
  boardInputPoll(&in);

  // Idle policy first: a sleeping device draws no frame, and the touch that
  // wakes it must not also turn a card.
  if (in.touchDown || in.touchBegan || in.touchEnded || in.tap || in.aDown ||
      in.aPressed || in.aLong || in.bPressed)
    powerNoteActivity();
  if (powerIdle(appSettings.brightness)) return;

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
    // The controls beside and above the deck are tested before the deck, so
    // their padded targets win where they overlap the cards.
    if (in.tap && uiSettingsLinkHit(in.x, in.y)) {
      holding = false;
      menuCursor = 1;
      settingsCursor = 0;
      resetArmed = false;
      go(SCR_SETTINGS);
      break;
    }
    if (in.tap) {
      const int8_t mode = uiDrawModeHit(in.x, in.y);
      if (mode >= 0) {
        const bool one = mode == 0;
        if (one != appSettings.singleCard) {
          appSettings.singleCard = one;
          settingsSave();
        }
        holding = false;
        break;
      }
    }
    // A single card is picked, not shuffled: one touch on the deck draws it.
    // The touch still stirs the entropy, the same as a hold would.
    if (appSettings.singleCard && in.tap && uiDeckHit(in.x, in.y)) {
      entropyStir(((uint32_t)in.x << 16) | (uint16_t)in.y, now);
      startCut();
      break;
    }
    // The deck's hit area starts at 88 and the pills end at 96, so the pills
    // are tested first here too: a finger on one must not start a shuffle.
    if (in.touchBegan && uiDrawModeHit(in.x, in.y) < 0 &&
        !uiSettingsLinkHit(in.x, in.y) && uiDeckHit(in.x, in.y)) {
      holding = true;
      holdStart = now;
    }
    float progress = 0;
    if (holding && in.touchDown) {
      entropyStir(((uint32_t)in.x << 16) | (uint16_t)in.y, now);
      progress = (now - holdStart) / (float)SHUFFLE_MS;
      if (progress > 1) progress = 1;
    }
    if (holding && in.touchEnded) {
      holding = false;
      progress = (now - holdStart) / (float)SHUFFLE_MS;
      if (progress >= 1.0f) {
        entropyStir(now - holdStart, entropyStirs());
        startCut();
      }
      progress = 0;
    }
    uiDeck(now, holding && in.touchDown, progress);
    if (in.bPressed) { menuCursor = 0; go(SCR_MENU); }
    // BOOT on the deck: a quick draw for the impatient (hardware noise only).
    if (in.aPressed) startCut();
    break;
  }

  case SCR_MENU:
    uiMenu(menuCursor);
    if (in.tap) {
      const int8_t item = uiMenuHit(in.x, in.y);
      if (item >= 0) { menuCursor = (uint8_t)item; openMenuItem(menuCursor); }
      else { go(SCR_DECK); }
    } else if (in.aPressed) {
      menuCursor = (uint8_t)((menuCursor + 1) % 2);
    } else if (in.bPressed) {
      openMenuItem(menuCursor);
    } else if (in.aLong) {
      go(SCR_DECK);
    }
    break;

  case SCR_HELP:
    uiHelp();
    if (in.tap || in.aPressed || in.bPressed) go(SCR_MENU);
    break;

  case SCR_SETTINGS: {
    uiSettings(appSettings, settingsCursor, resetArmed);
    const int8_t row = uiSettingsHit(in.x, in.y);
    // Brightness is dragged: the finger sets the level where it lands, and it
    // keeps setting it until it lifts. Every other row is a tap.
    static bool brightDrag = false;
    if (in.touchBegan && row == SET_ROW_BRIGHT) {
      settingsCursor = SET_ROW_BRIGHT;
      resetArmed = false;
      brightDrag = true;
    }
    if (brightDrag && (in.touchDown || in.touchEnded)) {
      appSettings.brightness = uiBrightnessAtX(in.x);
      settingsApplyHardware();
    }
    const bool dragEnded = brightDrag && in.touchEnded;
    if (dragEnded) {
      brightDrag = false;
      settingsSave();
    }
    if (in.tap && !dragEnded) {
      if (row >= 0) {
        if (row != (int8_t)settingsCursor) resetArmed = false;
        settingsCursor = (uint8_t)row;
        adjustSetting();
      } else {
        resetArmed = false;
        go(SCR_MENU);
      }
    } else if (in.aPressed) {
      settingsCursor = (uint8_t)((settingsCursor + 1) % SETTINGS_ROWS);
      resetArmed = false;
    } else if (in.bPressed) {
      adjustSetting();
    } else if (in.aLong) {
      resetArmed = false;
      go(SCR_MENU);
    }
    break;
  }

  case SCR_DEAL: {
    const float p = age / (float)dealMs();
    // One tick as each card leaves the deck (uiDeal starts card i at i*0.22).
    while (dealt < spread.count && p >= dealt * 0.22f) dealt++;
    uiDeal(p, spread.count);
    if (age >= dealMs()) {
      // One card has no spread to land on. It arrives face down, full size.
      if (spread.count == 1) { cardPos = 0; go(SCR_CARD); }
      else go(SCR_SPREAD);
    }
    break;
  }

  case SCR_GATHER: {
    const float p = age / (float)gatherMs();
    while (dealt < spread.count && p >= dealt * 0.22f) dealt++;
    uiGather(p, spread.count);
    if (age >= gatherMs()) go(SCR_DECK);
    break;
  }

  case SCR_SPREAD: {
    // Closing a reading throws it away: nothing about a spread is stored. So
    // the mark below it arms on the first tap and acts on the second, and the
    // arming expires on its own rather than waiting there for a stray finger.
    // An arming from a previous visit to this screen never counts.
    static uint32_t armedAt = 0;
    const bool armed = armedAt != 0 && armedAt >= enterMs && now - armedAt < 4000;
    if (!armed) armedAt = 0;
    uiSpread(spread, -1, 0, armed);
    if (in.tap && uiReadingResetHit(in.x, in.y)) {
      if (!armed) {
        armedAt = now;
      } else {
        armedAt = 0;
        holding = false;
        dealt = 0;
        go(SCR_GATHER);
      }
      break;
    }
    if (in.tap || in.aPressed || in.bPressed) armedAt = 0;
    if (in.tap) {
      const int8_t slot = uiSlotHit(spread, in.x, in.y);
      if (slot >= 0) {
        if (!spread.revealed[slot]) startFlip((uint8_t)slot);
        else zoomCard((uint8_t)slot);
      }
    }
    if (in.aPressed) {
      // BOOT: turn the next card, or walk into the first card once all are up.
      int8_t next = -1;
      for (uint8_t i = 0; i < spread.count; i++) if (!spread.revealed[i]) { next = (int8_t)i; break; }
      if (next >= 0) startFlip((uint8_t)next);
      else zoomCard(0);
    }
    if (in.bPressed) {
      // The inner reading needs an arc, elements, a pair and a quintessence.
      // A single card has none of those, so PWR only turns it.
      if (allRevealed()) openInner();
      else {
        for (uint8_t i = 0; i < spread.count; i++) if (!spread.revealed[i]) { startFlip(i); break; }
      }
    }
    break;
  }

  case SCR_FLIP: {
    const float p = age / (float)(spread.count == 1 ? FLIP_ONE_MS : FLIP_MS);
    if (spread.count == 1) uiCardFlip(spread, 0, p < 1 ? p : 1);
    else uiSpread(spread, flipSlot, p < 1 ? p : 1);
    if (age >= (spread.count == 1 ? FLIP_ONE_MS : FLIP_MS)) {
      spread.revealed[flipSlot] = true;
      flipSlot = -1;
      go(spread.count == 1 ? SCR_CARD : SCR_SPREAD);
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
    // A single card turns itself after a beat. Nothing else can happen on
    // this screen, so asking for a tap only adds a step.
    if (spread.count == 1 && !spread.revealed[cardPos] && age >= CARD_DWELL_MS) {
      startFlip(cardPos);
      break;
    }
    if ((in.tap || in.aPressed) && !spread.revealed[cardPos]) startFlip(cardPos);
    else if (in.tap || in.aPressed) go(SCR_MEANING);
    else if (in.swipeLeft) { if (cardPos + 1 < spread.count) openCard(cardPos + 1); else backToSpread(); }
    else if (in.swipeRight) { if (cardPos > 0) openCard(cardPos - 1); else backToSpread(); }
    if (in.bPressed) openInner();
    break;

  case SCR_MEANING:
    uiMeaning(spread, cardPos);
    if (in.tap) backToSpread();
    else if (in.aPressed) { if (cardPos + 1 < spread.count) openCard(cardPos + 1); else backToSpread(); }
    else if (in.swipeLeft) { if (cardPos + 1 < spread.count) openCard(cardPos + 1); else backToSpread(); }
    else if (in.swipeRight) { if (cardPos > 0) openCard(cardPos - 1); else backToSpread(); }
    if (in.bPressed) openInner();
    break;

  case SCR_INNER:
    uiInner(spread, innerPage, innerPages);
    if (in.tap || in.swipeLeft || in.aPressed) {
      if (innerPage + 1 < innerPages) innerPage++;
      else backToSpread();
    } else if (in.swipeRight && innerPage > 0) {
      innerPage--;
    }
    if (in.bPressed) backToSpread();
    break;
  }

  gfx->flush();
}
