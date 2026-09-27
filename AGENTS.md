# AGENTS.md — Malayalam-speaking ESP32 Desktop Pet

This file is the authoritative build spec for any coding agent (opencode) working
in this repository. Read this file fully before writing or editing any code.
Other `.md` files in this repo (`docs/*.md`, `HARDWARE.md`, `README.md`) are
referenced from here and must stay consistent with this file. If any instruction
in another file conflicts with this file, this file wins.

## 1. What we're building

A desktop pet powered by an ESP32-S3 dev board. It has a face on a small OLED
screen, listens through a microphone, sends what it hears to Google Gemini,
and speaks the reply out loud in Malayalam through a speaker. The face must
visibly animate through the whole interaction (idle, listening, thinking,
talking, error) — this is not optional polish, it is a core requirement (see
§4).

The system has two independent codebases in one repo:

- `firmware/` — C++ (Arduino/PlatformIO) code flashed onto the ESP32-S3.
- `server/` — Python relay server (FastAPI) that does Speech-to-Text, calls
  Gemini, does Text-to-Speech, and returns audio to the ESP32.

They communicate over plain HTTP on the local WiFi network. There is no cloud
hosting requirement — the server runs on a laptop/Pi on the same network as
the ESP32.

## 2. Non-negotiable requirements

1. **OLED face animation is mandatory.** The pet must show a distinct,
   continuously-updating facial animation for each state it's in (idle,
   listening, thinking, talking, error/offline). A static image or a single
   "happy face" bitmap is not acceptable — see §4 for the exact state list
   and animation behavior required.
2. **The only LLM used is Google Gemini** (via the `google-generativeai` /
   Gemini API). Do not substitute OpenAI, Claude, or any other model.
3. **Spoken language is Malayalam** (`ml-IN`) for both understanding the
   user (via STT or Gemini audio input) and speaking back (via TTS). System
   prompts to Gemini must explicitly instruct it to reply only in Malayalam.
4. **Build the server even though this development machine has no Python
   installed.** Do not skip, stub, or "simplify" `server/` because Python
   isn't available here to test it locally. Write it completely and
   correctly against the spec in `docs/SERVER.md`; it will be run on the
   target machine, which does have Python 3.10+. If you cannot execute
   `pip install` or `uvicorn` in this environment, that's expected —
   validate the code by careful reading, type-consistency, and (if a
   sandboxed Python happens to be available) `python -m py_compile` on each
   file, rather than skipping the work.
5. **No hard failures.** Every network call (WiFi, HTTP to server, Gemini,
   Google STT/TTS) must have a timeout and a defined fallback behavior. The
   pet should never hang forever or crash-loop; on any failure it shows the
   error face (§4) and recovers automatically on the next interaction. See
   §7 for the full failure-handling checklist.
6. **Pin assignments must match `HARDWARE.md` exactly.** That file was
   derived directly from the wiring diagram for this build and is
   authoritative — do not "correct" it from a generic tutorial's pinout.

## 3. Repo layout

```
desktop-pet/
├── AGENTS.md                  (this file)
├── README.md                  human-readable overview + quick start
├── HARDWARE.md                wiring table, authoritative pin map, BOM
├── docs/
│   ├── FIRMWARE.md            firmware architecture, state machine, files
│   ├── SERVER.md              relay server architecture, endpoints
│   ├── API.md                 Gemini + Google Cloud STT/TTS integration spec
│   └── TESTING.md             acceptance checklist, per-module test plan
├── firmware/                  PlatformIO project — flashed to ESP32-S3
│   ├── platformio.ini
│   ├── include/
│   │   └── config.h           WiFi creds, server URL, pin defs (placeholders)
│   └── src/
│       ├── main.cpp
│       ├── pins.h
│       ├── wifi_manager.cpp/.h
│       ├── audio_capture.cpp/.h
│       ├── audio_playback.cpp/.h
│       ├── net_client.cpp/.h
│       └── pet_face.cpp/.h    OLED animation state machine (mandatory, §4)
└── server/                    Python relay server — runs on laptop/Pi
    ├── requirements.txt
    ├── .env.example
    ├── app.py
    ├── stt.py
    ├── llm.py
    └── tts.py
```

Build in this order: `HARDWARE.md` pins → `pet_face.cpp/.h` (OLED animation,
testable standalone with no network) → `wifi_manager` → `audio_capture` /
`audio_playback` → `server/` (all four files) → `net_client.cpp/.h` wiring
firmware to server → `main.cpp` tying the whole state machine together. This
order lets each piece be sanity-checked before it depends on anything else.

## 4. OLED animation requirement (must-have, do not skip)

The 128x64 SSD1306 OLED is the pet's face and is the main way the user knows
the pet is alive and doing something. Implement `pet_face.h/.cpp` as a small
state machine with at least these states, each with a distinct animated
(not static) look:

| State      | Trigger                                    | Animation behavior |
|------------|---------------------------------------------|---------------------|
| `IDLE`     | Default / after a reply finishes             | Eyes blink every 3–5s (randomized interval), occasional slow "look around" — matches the friendly rounded-eye face style in the wiring diagram's OLED mockup |
| `LISTENING`| BOOT button pressed, mic recording active    | Eyes widen slightly; a small animated waveform/pulse under the eyes reacting to the fact audio is being captured |
| `THINKING` | Audio sent to server, awaiting HTTP response | Eyes narrow slightly; animated "loading" cue (e.g. rotating dots or pulsing) so the user knows it's not frozen |
| `TALKING`  | Playing back the TTS audio reply             | Mouth shape animates open/closed in sync with playback duration (simple timer-driven mouth-open/mouth-closed alternation is sufficient — do not attempt true lip-sync) |
| `ERROR`    | WiFi lost, server unreachable, or API error  | A distinct "confused/sad" face (e.g. X X eyes or a wavy mouth) shown for a few seconds, then automatically return to `IDLE` |

Implementation requirements:
- Use `Adafruit_GFX` + `Adafruit_SSD1306` (I2C, address `0x3C` unless the
  module you have differs — make this a `#define` in `pins.h`, not a magic
  number scattered through the code).
- Drive animation from `millis()`-based timers, never `delay()` — the face
  must keep animating while WiFi/HTTP/I2S work happens elsewhere in
  `loop()`. This is the main reason the firmware must be non-blocking (§7).
- Put all animation frame logic in `pet_face.cpp` behind a small API like
  `petFace.setState(PetState::LISTENING); petFace.update();` called every
  loop iteration — `main.cpp` should only ever call `setState()`, never draw
  pixels directly.
- Confirm this module works completely standalone (flash firmware with only
  the OLED wired up, cycle through all five states on a timer) before wiring
  it to the rest of the system. This is test #1 in `docs/TESTING.md`.

## 5. Hardware summary

Full pin table lives in `HARDWARE.md` — read it before touching
`pins.h`. Board is an ESP32-S3-WROOM-1 dev board. Components: INMP441 I2S
microphone, MAX98357A I2S amplifier + 2W 4Ω speaker, 0.96" SSD1306 OLED
(I2C, 128x64), BOOT button (GPIO0, already on-board) used as the
push-to-talk trigger.

## 6. Software architecture (summary — see docs/FIRMWARE.md and docs/SERVER.md)

```
[BOOT button press]
        │
        ▼
ESP32 records ~3–5s from INMP441 over I2S  (pet_face → LISTENING)
        │  raw PCM (16kHz, 16-bit, mono)
        ▼
POST multipart audio → http://<server-ip>:8000/talk   (pet_face → THINKING)
        │
        ▼  (server/app.py)
Google Speech-to-Text (ml-IN) → Malayalam transcript
        │
        ▼
Gemini API, system prompt forces Malayalam pet-personality reply
        │
        ▼
Google Text-to-Speech (ml-IN) → reply audio (WAV, 16kHz)
        │
        ▼
HTTP response body = WAV bytes
        │
        ▼
ESP32 streams response to MAX98357A over I2S           (pet_face → TALKING)
        │
        ▼
back to IDLE
```

## 7. Reliability / "must not fail" checklist

Apply these everywhere, not just where convenient:

- **Non-blocking main loop.** No `delay()` longer than a few ms anywhere in
  `main.cpp` or `pet_face.cpp`. Long operations (recording, HTTP, playback)
  must not prevent `petFace.update()` from being called regularly, or the
  face will freeze and look broken.
- **WiFi reconnect.** `wifi_manager` must detect disconnects and retry with
  backoff, not just connect once in `setup()`. Show `ERROR` state while
  disconnected.
- **HTTP timeouts.** Every `HTTPClient` call in `net_client.cpp` needs an
  explicit timeout (e.g. 15s) — an unreachable server must not hang the
  device. On timeout or non-200 response, go to `ERROR` state and return to
  `IDLE`, don't retry in a tight loop.
- **Server-side try/except around every external call** (`stt.py`,
  `llm.py`, `tts.py`) — a Google API hiccup or a Gemini rate-limit response
  must return a clean HTTP error to the ESP32, never a raw Python traceback
  or a hung request.
- **Empty/failed transcription handling.** If STT returns no text (silence,
  noise), the server should short-circuit and return a small "didn't catch
  that" Malayalam audio clip (or a defined empty response the firmware
  recognizes) rather than sending empty text to Gemini.
- **Config validation on boot.** `main.cpp` should sanity-check that
  `WIFI_SSID`, `WIFI_PASSWORD`, and `SERVER_URL` in `config.h` are non-empty
  placeholders before proceeding, and print a clear Serial error if not.
  `app.py` should do the same for `GEMINI_API_KEY` and
  `GOOGLE_APPLICATION_CREDENTIALS` at startup, refusing to start with a
  clear message rather than failing on the first request.
- **Recording length bounds.** Cap recording at a fixed max duration
  (e.g. 6s) regardless of button behavior, so a stuck button can't fill
  memory or hang the device.

## 8. Secrets

Never hardcode real API keys or WiFi passwords into files that get
committed. `firmware/include/config.h` and `server/.env` both must ship as
`*.example` templates with placeholder values, and both must be listed in
`.gitignore`. Document this clearly in `README.md`.

## 9. Definition of done

- [ ] `pet_face` module runs standalone, cycling through all 5 states with
      visible animation (not static frames), verified per `docs/TESTING.md`.
- [ ] Firmware connects to WiFi, recovers from disconnects, shown via
      `ERROR` state.
- [ ] BOOT button triggers a bounded-length recording, visible via
      `LISTENING` state.
- [ ] Server exposes `/health` and `/talk`, both implemented exactly as in
      `docs/SERVER.md`, with try/except around every external call.
- [ ] `/talk` round-trip works end-to-end: audio in → Malayalam transcript →
      Gemini reply (Malayalam only) → Malayalam TTS audio → played on the
      speaker, with `THINKING` and `TALKING` states shown at the right times.
- [ ] Every failure mode in `docs/TESTING.md` §"Failure-mode tests" has been
      exercised and results in `ERROR` state + automatic recovery, not a
      hang or crash.
- [ ] No secrets committed; `.example` files present and documented.
