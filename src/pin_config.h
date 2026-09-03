// Waveshare ESP32-S3-Touch-AMOLED-1.75 round board. Every value is copied
// from sibling firmware that runs on the exact board; do not change a pin
// without a source.
#pragma once

#if !defined(BOARD_ROUND_175)
#error "tinyarcana targets only the Waveshare 1.75 round board"
#endif

// Waveshare ESP32-S3-Touch-AMOLED-1.75 (no C): round 466x466 CO5300 over
// QSPI, CST9217 touch, AXP2101 PMU. Values from esp32-lofiair/src/pin_config.h
// (LOFIAIR_AMOLED_175C profile, sourced from Waveshare's schematic) and
// esp32-thinking-orbs/src/pin_config.h, which agree.
#define LCD_SDIO0 4
#define LCD_SDIO1 5
#define LCD_SDIO2 6
#define LCD_SDIO3 7
#define LCD_SCLK  38
#define LCD_CS    12
#define LCD_RESET 39
#define LCD_WIDTH  466
#define LCD_HEIGHT 466
// CO5300 RAM is wider than the glass. Waveshare's demo uses a 6 column offset.
#define LCD_COL_OFFSET 6

#define IIC_SDA 15
#define IIC_SCL 14
#define AXP2101_ADDR 0x34
#define CST9217_ADDR 0x5A
#define TP_INT       11
#define TP_RST       40   // separate from LCD_RESET, not shared

// BOOT is a GPIO. PWR is the AXP2101 power key, read as a PMU IRQ; a long
// hold still powers the board off in hardware.
#define BTN_BOOT 0

// ES8311 codec and amplifier enable. MCLK is intentionally not driven; the
// codec is clocked from BCLK. See audio.cpp.
#define I2S_BCK_IO 9
#define I2S_WS_IO  45
#define I2S_DO_IO  8
#define PA_EN      46
#define ES8311_I2C_ADDR 0x18
