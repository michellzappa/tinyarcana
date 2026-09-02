#pragma once

#include <Arduino.h>

struct InputFrame {
  bool aPressed;      // BOOT rising edge
  bool aLong;         // BOOT held >= 800 ms (once per hold)
  bool bPressed;      // PWR short press (release < 800 ms)
  bool touchDown;     // finger on glass right now
  bool touchBegan;    // this frame
  bool touchEnded;    // this frame; tap/swipe flags are valid
  bool tap;
  bool swipeLeft;
  bool swipeRight;
  int16_t x, y;       // current or last finger position, panel coords
  int16_t startX, startY;
  uint32_t holdMs;    // time the finger has been down
};

void boardInputBegin();
void boardInputPoll(InputFrame *out);
bool boardTouchPresent();
