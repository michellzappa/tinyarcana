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

// ---------------- Idle policy ----------------
//
// The panel is the largest continuous draw on this board and it ran at the
// reader's brightness forever. On battery the device now dims, then stops the
// chip between touches. On USB it only dims: a device on a cable is a device
// someone is looking at.
//
// Call powerNoteActivity() for any touch or button, then powerIdle() once a
// frame with the brightness the reader chose.
void powerNoteActivity();
// Returns true when the caller must skip this frame: the device was asleep,
// and the input that woke it only woke it.
bool powerIdle(uint8_t awake);
