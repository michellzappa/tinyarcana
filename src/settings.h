#pragma once

#include <Arduino.h>

struct AppSettings {
  uint8_t deckId;
  uint8_t brightness;
  // Draw one card instead of past / present / future. Set on the deck screen,
  // where the ritual it changes actually happens.
  bool singleCard;
  // BCP-47-ish code naming a directory under /lang on the filesystem. The
  // packs decide which languages exist, so the firmware never enumerates them.
  char lang[6];
};

extern AppSettings appSettings;

void settingsBegin();
void settingsApplyHardware();
void settingsSave();
void settingsReset();
