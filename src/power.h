// Battery and charger state, read from the AXP2101.
//
// The fuel gauge moves slowly and every read is I2C traffic, so the state is
// polled on a timer and cached. The UI reads the cache, never the PMU.
#pragma once

#include <Arduino.h>

struct PowerState {
  bool valid = false;      // the PMU answered at boot
  bool vbus = false;       // USB supply present
  bool battery = false;    // the PMU senses a cell
  bool charging = false;
  int8_t percent = -1;     // 0..100, or -1 when the gauge says nothing
};

// Reads the PMU at most every 10 s and returns the cached state.
const PowerState &powerPoll();
