#pragma once

#include <Arduino.h>

struct AppSettings {
  uint8_t deckId;
  uint8_t brightness;
  bool showHiddenCard;
};

extern AppSettings appSettings;

void settingsBegin();
void settingsApplyHardware();
void settingsSave();
void settingsReset();
