#include "power.h"

#include "board_display.h"

static PowerState state;
static uint32_t lastPoll = 0;

const PowerState &powerPoll() {
  const uint32_t now = millis();
  if (lastPoll != 0 && now - lastPoll < 10000) return state;
  lastPoll = now;

  PowerState next;
  next.valid = pmuOk;
  if (pmuOk) {
    next.vbus = PMU.isVbusIn();
    next.battery = PMU.isBatteryConnect();
    next.charging = next.battery && PMU.isCharging();
    // XPowersLib returns -1 whenever it does not trust the gauge. Keep that
    // as -1 rather than drawing a fill the cell cannot support.
    const int percent = next.battery ? PMU.getBatteryPercent() : -1;
    next.percent = (percent >= 0 && percent <= 100) ? (int8_t)percent : -1;
  }

  if (next.valid != state.valid || next.vbus != state.vbus ||
      next.battery != state.battery || next.charging != state.charging ||
      next.percent != state.percent) {
    Serial.printf("power: source=%s battery=%s charge=%s level=%d%%\n",
                  next.vbus ? "USB" : (next.battery ? "BAT" : "UNKNOWN"),
                  next.battery ? "yes" : "no",
                  next.charging ? "yes" : "no", (int)next.percent);
  }
  state = next;
  return state;
}
