#include "power.h"

#include <driver/gpio.h>
#include <esp_sleep.h>

#include "board_display.h"
#include "pin_config.h"

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

// ---------------- Idle policy ----------------
//
// A reader looks at a page for a while, so the timers are generous: a spread
// that dimmed after twenty seconds would dim while it was being read.
static const uint32_t DIM_BATTERY_MS = 45000;
static const uint32_t DIM_USB_MS = 180000;
static const uint32_t SLEEP_BATTERY_MS = 180000;
// The chip wakes on a touch or on BOOT immediately, and on this timer anyway.
// The timer is what reads the PWR key: PWR is an AXP2101 interrupt over I2C,
// not a pin, so nothing can wake the chip on it. The IRQ latches, so a press
// during sleep is still there to be found. The timer is also the way out if a
// GPIO wake ever fails: the device polls four times a second and cannot get
// stuck asleep.
static const uint64_t SLEEP_TICK_US = 250000;

static uint32_t lastActivityMs = 0;
static bool asleep = false;
static uint8_t applied = 0;

void powerNoteActivity() { lastActivityMs = millis(); }

static void applyBrightness(uint8_t b) {
  if (b == applied) return;
  applied = b;
  boardSetBrightness(b);
}

// The PWR key, read the way board_input.cpp reads it. Consuming the IRQ here
// means the press that wakes the device does not also act on the screen it
// wakes to.
static bool pwrKeyLatched() {
  if (!pmuOk) return false;
  PMU.getIrqStatus();
  const bool key = PMU.isPekeyShortPressIrq() || PMU.isPekeyLongPressIrq();
  PMU.clearIrqStatus();
  return key;
}

static bool wakeAsked() {
  return digitalRead(TP_INT) == LOW || digitalRead(BTN_BOOT) == LOW ||
         pwrKeyLatched();
}

static void sleepTick() {
  static bool configured = false;
  if (!configured) {
    configured = true;
    // Level, not edge: light sleep on the S3 wakes on a level. Both lines idle
    // high, so LOW is a finger on the glass or a thumb on BOOT.
    gpio_wakeup_enable((gpio_num_t)TP_INT, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable((gpio_num_t)BTN_BOOT, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();
  }
  esp_sleep_enable_timer_wakeup(SLEEP_TICK_US);
  esp_light_sleep_start();
}

bool powerIdle(uint8_t awake) {
  const uint32_t now = millis();
  if (lastActivityMs == 0) lastActivityMs = now;

  if (asleep) {
    if (wakeAsked()) {
      asleep = false;
      lastActivityMs = now;
      applyBrightness(awake);
      Serial.println("idle: awake");
      return true;   // the touch that woke it only woke it
    }
    sleepTick();
    return true;
  }

  const PowerState &p = powerPoll();
  // A board that reports no cell is on the cable, whatever the gauge says:
  // vbus is the honest signal, and sleeping a device on mains helps nobody.
  const bool onBattery = p.valid && !p.vbus;
  const uint32_t idle = now - lastActivityMs;

  if (onBattery && idle >= SLEEP_BATTERY_MS) {
    asleep = true;
    applyBrightness(0);   // black on an AMOLED is pixels off
    Serial.println("idle: sleeping");
    sleepTick();
    return true;
  }

  const uint32_t dimAfter = onBattery ? DIM_BATTERY_MS : DIM_USB_MS;
  uint8_t dim = (uint8_t)(awake / 4);
  if (dim < 24) dim = 24;
  if (dim > awake) dim = awake;
  applyBrightness(idle >= dimAfter ? dim : awake);
  return false;
}
