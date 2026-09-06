#include "settings.h"

#include <Preferences.h>
#include <stdio.h>

#include "board_display.h"
#include "deck.h"

static const uint8_t DEFAULT_BRIGHTNESS = 200;

AppSettings appSettings = {0, DEFAULT_BRIGHTNESS, false, "en"};

static Preferences prefs;

void settingsBegin() {
  prefs.begin("tinyarcana", false);
  appSettings.deckId = prefs.getUChar("deck", 0);
  if (appSettings.deckId >= DECK_COUNT) appSettings.deckId = 0;
  appSettings.brightness = prefs.getUChar("bright", DEFAULT_BRIGHTNESS);
  appSettings.singleCard = prefs.getBool("single", false);
  prefs.getString("lang", appSettings.lang, sizeof appSettings.lang);
  if (!appSettings.lang[0]) snprintf(appSettings.lang, sizeof appSettings.lang, "en");
}

void settingsApplyHardware() {
  boardSetBrightness(appSettings.brightness);
}

void settingsSave() {
  prefs.putUChar("deck", appSettings.deckId);
  prefs.putUChar("bright", appSettings.brightness);
  prefs.putBool("single", appSettings.singleCard);
  prefs.putString("lang", appSettings.lang);
}

void settingsReset() {
  appSettings = {0, DEFAULT_BRIGHTNESS, false, "en"};
  settingsSave();
  settingsApplyHardware();
}
