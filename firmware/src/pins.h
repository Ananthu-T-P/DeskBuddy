#pragma once
// Authoritative pin map — must stay in sync with HARDWARE.md.
// Nothing else in this codebase should contain a raw GPIO number.

// INMP441 I2S microphone
#define PIN_MIC_WS   4   // word select / LRCLK
#define PIN_MIC_SCK  5   // bit clock / BCLK
#define PIN_MIC_SD   6   // data out (mic -> ESP32)

// MAX98357A I2S amplifier
#define PIN_AMP_BCLK 7
#define PIN_AMP_LRC  15
#define PIN_AMP_DIN  16  // ESP32 -> amp

// 0.96" SSD1306 OLED (I2C) — confirmed pins, do not change
#define PIN_OLED_SDA 41
#define PIN_OLED_SCL 42
#define OLED_I2C_ADDRESS 0x3C   // try 0x3D if the display doesn't init
#define OLED_WIDTH  128
#define OLED_HEIGHT 64

// Push-to-talk (on-board BOOT button)
#define PIN_BUTTON 0
