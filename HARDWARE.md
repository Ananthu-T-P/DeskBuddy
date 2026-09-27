# HARDWARE.md — Wiring & Pinout (authoritative)

Derived directly from the project's wiring diagram. If firmware code and this
file ever disagree, this file is correct — fix the code, not the doc.

Board: **ESP32-S3-WROOM-1** dev board (38/44-pin style breakout shown in the
diagram, USB-C x2, on-board RGB LED and BOOT/RST buttons).

## Bill of materials

| Component | Role |
|---|---|
| ESP32-S3-WROOM-1 dev board | Main controller — WiFi, I2S, I2C |
| INMP441 | I2S MEMS microphone (voice input) |
| MAX98357A | I2S class-D amplifier breakout |
| 2W 4Ω speaker | Audio output |
| 0.96" SSD1306 OLED, 128x64, I2C | Pet face display |
| On-board BOOT button (GPIO0) | Push-to-talk trigger |

## Pin map

### INMP441 I2S microphone

| Mic pin | ESP32-S3 pin | Notes |
|---|---|---|
| VDD | 3V3 | |
| GND | GND | |
| L/R | GND | Ties to left channel |
| WS (word select / LRCLK) | GPIO4 | |
| SCK (bit clock / BCLK) | GPIO5 | |
| SD (data out) | GPIO6 | Mic → ESP32 |

### MAX98357A I2S amplifier

| Amp pin | ESP32-S3 pin | Notes |
|---|---|---|
| VIN | 3V3 | |
| GND | GND | |
| BCLK | GPIO7 | |
| LRC (word select) | GPIO15 | |
| DIN | GPIO16 | ESP32 → amp |
| SD | not connected | Floating = amplifier enabled |
| GAIN | not connected | Floating = default (~9dB) gain |

### 0.96" SSD1306 OLED (I2C)

| OLED pin | ESP32-S3 pin | Notes |
|---|---|---|
| GND | GND | |
| VCC | 3V3 | |
| SDA | **GPIO41** | Confirmed pin — do not change |
| SCL | **GPIO42** | Confirmed pin — do not change |

I2C address: `0x3C` (standard for this 0.96" module — verify with an I2C
scanner sketch if the display doesn't init; some units ship as `0x3D`).

### Controls

| Control | ESP32-S3 pin | Notes |
|---|---|---|
| Push-to-talk | GPIO0 (on-board BOOT button) | Active-low, needs `INPUT_PULLUP` |

## Power notes

- Mic and amp both run off 3V3 from the ESP32-S3 board. This is fine for
  prototyping; if the speaker sounds weak/distorted at higher volumes,
  consider powering the MAX98357A `VIN` from 5V instead (it accepts
  2.5V–5.5V) — but keep I2S logic pins as-is since they're separate from
  power.
- I2S bus is shared logically (mic and amp use different BCLK/WS pins here,
  each on its own I2S peripheral) — the ESP32-S3 has two I2S peripherals, so
  use one for the mic (input) and the other for the amp (output). Do not try
  to time-share a single I2S peripheral between simultaneous record and
  playback; this project never records and plays back at the same instant,
  so the pins as wired are sufficient, but keep the two logical I2S buses
  (mic vs. amp) configured as separate `i2s_port_t` instances in code.

## GPIO summary table

| GPIO | Used by |
|---|---|
| 0 | BOOT button (push-to-talk) |
| 4 | Mic WS |
| 5 | Mic SCK |
| 6 | Mic SD |
| 7 | Amp BCLK |
| 15 | Amp LRC |
| 16 | Amp DIN |
| 41 | OLED SDA |
| 42 | OLED SCL |
