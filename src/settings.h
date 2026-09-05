#pragma once

#include <Arduino.h>

struct AppSettings {
  uint8_t deckId;
  uint8_t brightness;
  bool showHiddenCard;
  bool singleCard;   // draw one card instead of past / present / future
};

extern AppSettings appSettings;

void settingsBegin();
void settingsApplyHardware();
void settingsSave();
void settingsReset();
