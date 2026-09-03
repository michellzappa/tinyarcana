// Host build shim. tarot_engine.h includes <Arduino.h> for the integer types
// only; nothing in the engine touches the Arduino runtime. This lets
// tools/dump_readings.cpp compile the firmware's engine unmodified on macOS
// or Linux. Do not add anything here that the firmware would not also have.
#pragma once

#include <stddef.h>
#include <stdint.h>
