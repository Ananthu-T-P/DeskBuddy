# Desktop Pet — ESP32-S3 + Gemini (Malayalam)

An ESP32-S3 desktop pet with an animated OLED face. Press the button, talk
to it, and it replies out loud in Malayalam using the Google Gemini API for
everything — hearing, thinking, and speaking. One free API key total, no
Google Cloud account needed.

See `AGENTS.md` for the full build spec if you're an AI coding agent working
on this repo. Humans, keep reading.

## What's in this repo

- `firmware/` — ESP32-S3 code (PlatformIO). Flash this to the board.
- `server/` — Python relay server. Runs on a laptop or Raspberry Pi on the
  same WiFi network as the ESP32. **Requires Python 3.10+** — this repo may
  have been written on a machine without Python installed, but it must be
  run on one that has it.
- `HARDWARE.md` — exact wiring/pinout for this build.
- `docs/` — detailed specs for firmware, server, the Gemini/Google Cloud
  integration, and the test checklist.

## Quick start

### 1. Wire it up

Follow `HARDWARE.md` exactly — pin numbers there are confirmed against the
actual wiring diagram for this build, including the OLED on GPIO41 (SDA) /
GPIO42 (SCL).

### 2. Get your API key

- **Gemini API key** — from Google AI Studio (aistudio.google.com → "Get
  API key"). This single key covers hearing, thinking and speaking
  (Malayalam throughout). No Google Cloud project or service-account file
  needed. Details: `docs/API.md`.

### 3. Set up the server (on a machine with Python)

```bash
cd server
python3 -m venv venv
source venv/bin/activate        # Windows: venv\Scripts\activate
pip install -r requirements.txt
# open .env (already in the repo) and paste your Gemini API key
uvicorn app:app --host 0.0.0.0 --port 8000
```

Confirm it's up: open `http://<this-machine's-local-ip>:8000/health` from
another device on the same WiFi — should return `{"status":"ok"}`.

Find the local IP: `ipconfig` (Windows) or `ifconfig`/`ip addr` (Mac/Linux),
look under the WiFi adapter.

### 4. Configure and flash the firmware

```bash
cd firmware
# edit include/config.h (already in the repo): WiFi SSID/password,
# and SERVER_URL = http://<server-ip>:8000/talk
```

Open in PlatformIO, build, and upload to the ESP32-S3.

### 5. Use it

Press the BOOT button to talk. Watch the OLED — it should animate through
idle → listening → thinking → talking states as you interact (see
`docs/FIRMWARE.md` for exactly what each state looks like). Speak in
Malayalam (or any language — Gemini will still be instructed to reply in
Malayalam); the pet answers out loud through the speaker.

## Secrets

`firmware/include/config.h` and `server/.env` live in the repo WITH
PLACEHOLDER VALUES on purpose, so anyone cloning it sees the complete
structure. Fill in the real WiFi details / Gemini key only on the machine
that actually runs each part — and never commit or push those edits back
(AGENTS.md §8).

## Costs

Hardware is a one-time cost (~₹1000–1600 for the parts listed in
`HARDWARE.md`). Software is free at hobby-usage volume: the Gemini API free
tier covers hearing + brain + voice for dozens of daily interactions, and
running the server on your own laptop/Pi costs nothing. (New Google
accounts are sometimes asked to "enable billing" on the project to unlock
the free tier — nothing is charged within free-tier limits; set a budget
alert for certainty. Check `docs/API.md` for details.)

## Troubleshooting

See `docs/TESTING.md` for a step-by-step test plan (test the OLED alone,
then the mic alone, then the server alone, then the full loop) — this is
the fastest way to isolate a problem instead of debugging the whole chain
at once.
