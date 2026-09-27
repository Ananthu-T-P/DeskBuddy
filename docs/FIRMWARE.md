# docs/FIRMWARE.md — ESP32-S3 Firmware Spec

Target: **Arduino IDE sketch** at `firmware/Mahoraga/Mahoraga.ino` (the
folder name and `.ino` name must match — Arduino IDE requirement). Board:
"ESP32S3 Dev Module" (esp32 core by Espressif via Boards Manager). Tools
menu: set **"USB CDC On Boot" = Enabled** or Serial Monitor shows nothing.

## Libraries

Install via Arduino IDE Library Manager: **Adafruit SSD1306** and **Adafruit
GFX Library** (their Audio/BusIO dependencies install automatically).
Everything else ships with the esp32 Arduino core: `WiFi`, `HTTPClient`,
`WiFiClientSecure` (used when `SERVER_URL` is https://, e.g. behind a
tunnel), `Wire`, and the legacy `driver/i2s.h`.

- `Adafruit_GFX` + `Adafruit_SSD1306` — OLED face rendering.
- `HTTPClient` (bundled with the ESP32 Arduino core) — POST to the relay
  server. Supports both plain-HTTP LAN servers and HTTPS tunnel URLs.
- I2S: use the ESP32 Arduino core's built-in `driver/i2s.h` directly rather
  than a heavyweight audio framework — this project only needs raw PCM in
  and raw PCM/WAV out, not decoding/mixing. (Legacy I2S API: deprecation
  warnings from newer cores are expected and harmless.)

## File responsibilities

- **`Mahoraga.ino`** — `setup()`/`loop()` orchestration only. Owns the
  top-level state machine (`IDLE → LISTENING → THINKING → TALKING → IDLE`,
  with `ERROR` reachable from any state). Calls into the other modules;
  contains no I2S, HTTP, or pixel-drawing code directly. (Also holds the
  `FACE_SELFTEST` switch for `docs/TESTING.md` §1.)
- **`pins.h`** — every GPIO number and the OLED I2C address as named
  `#define`s, sourced from `HARDWARE.md`. Nothing else in the codebase
  should contain a raw pin number.
- **`wifi_manager.cpp/.h`** — connect, monitor, and reconnect WiFi
  non-blockingly. Exposes `wifiManager.isConnected()` and
  `wifiManager.update()` (call every loop iteration; internally
  rate-limits reconnect attempts).
- **`audio_capture.cpp/.h`** — configures the mic's I2S peripheral, records
  a bounded-duration clip into a buffer/PSRAM, exposes something like
  `audioCapture.startRecording()`, `audioCapture.isDone()`,
  `audioCapture.getBuffer()`.
- **`audio_playback.cpp/.h`** — configures the amp's I2S peripheral, plays
  a WAV byte buffer (skip the 44-byte header, stream PCM frames to I2S),
  exposes `audioPlayback.play(buffer, length)`,
  `audioPlayback.isPlaying()`. Should report progress/duration so
  `pet_face` can drive the `TALKING` mouth animation for the right length
  of time.
- **`net_client.cpp/.h`** — owns the `HTTPClient` POST to `SERVER_URL`,
  multipart/form-data encoding of the recorded audio, timeout handling,
  and returns the response body (WAV bytes) or a clear error code.
- **`pet_face.cpp/.h`** — the mandatory OLED animation state machine
  described in `AGENTS.md` §4. This is the one module that must work
  completely standalone before anything else is wired up.

## State machine (in `Mahoraga.ino`)

```
enum class PetState { IDLE, LISTENING, THINKING, TALKING, ERROR };
```

- On BOOT button press (debounced, `INPUT_PULLUP`, active low) while in
  `IDLE`: → `LISTENING`, start `audioCapture`.
- Recording finishes (max duration reached or configurable silence-timeout):
  → `THINKING`, call `netClient` (non-blocking — poll for the HTTP result
  each loop iteration rather than blocking on `http.POST()`'s return if the
  library allows it; if not, keep the blocking POST but make sure
  `petFace.update()` is called immediately before and after so the THINKING
  animation is at least drawn going in and doesn't appear to freeze for
  more than the HTTP call's duration — document the actual timeout in
  `pins.h`/`config.h` as `HTTP_TIMEOUT_MS`, default 15000).
- HTTP success with audio body: → `TALKING`, start `audioPlayback`.
- HTTP failure/timeout, or empty body: → `ERROR` for ~3s, then → `IDLE`.
- Playback finishes: → `IDLE`.
- WiFi disconnected at any point: → `ERROR` until `wifiManager` reconnects,
  then → `IDLE`.

Every state transition calls `petFace.setState(newState)` once — animation
detail lives entirely inside `pet_face.cpp`, not in `Mahoraga.ino`.

## Recording format

16kHz, 16-bit signed PCM, mono — matches what Google Speech-to-Text expects
for `LINEAR16` (see `docs/API.md`), so no resampling is needed between mic
capture and the STT call on the server.

## Config

`Mahoraga/config.h` (tracked with placeholder values — fill real values on
the machine that flashes/runs it; never push that edit back):

```cpp
#pragma once

#define WIFI_SSID "your-wifi-ssid"
#define WIFI_PASSWORD "your-wifi-password"
#define SERVER_URL "http://192.168.1.50:8000/talk"

#define HTTP_TIMEOUT_MS 15000
#define MAX_RECORDING_MS 6000
```

`Mahoraga.ino` must check at boot that these aren't left as the placeholder
strings and print a clear `Serial` error (and show `ERROR` on the OLED) if
they are, per `AGENTS.md` §7.
