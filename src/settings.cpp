#include "settings.h"

#include <Preferences.h>

#include "board_display.h"
#include "deck.h"

static const uint8_t DEFAULT_BRIGHTNESS = 200;

AppSettings appSettings = {0, DEFAULT_BRIGHTNESS, true, false};

static Preferences prefs;

void settingsBegin() {
  prefs.begin("tinyarcana", false);
  appSettings.deckId = prefs.getUChar("deck", 0);
  if (appSettings.deckId >= DECK_COUNT) appSettings.deckId = 0;
  appSettings.brightness = prefs.getUChar("bright", DEFAULT_BRIGHTNESS);
  appSettings.showHiddenCard = prefs.getBool("hidden", true);
  appSettings.singleCard = prefs.getBool("single", false);
}

void settingsApplyHardware() {
  boardSetBrightness(appSettings.brightness);
}

void settingsSave() {
  prefs.putUChar("deck", appSettings.deckId);
  prefs.putUChar("bright", appSettings.brightness);
  prefs.putBool("hidden", appSettings.showHiddenCard);
  prefs.putBool("single", appSettings.singleCard);
}

void settingsReset() {
  appSettings = {0, DEFAULT_BRIGHTNESS, true, false};
  settingsSave();
  settingsApplyHardware();
}
