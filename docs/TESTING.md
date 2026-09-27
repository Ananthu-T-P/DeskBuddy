# docs/TESTING.md — Test Plan & Acceptance Checklist

Test each module standalone, in this order, before integrating. This is the
fastest way to isolate a fault in a system with this many moving parts
(WiFi, two I2S peripherals, I2C, an external server, three external APIs).
Don't jump straight to the full end-to-end flow.

## 1. OLED face, standalone

Flash firmware with only the OLED wired (GPIO41/42, per `HARDWARE.md`).
Cycle `pet_face` through all five states on a timer (e.g. 3s each) instead
of from real triggers. Confirm:
- [ ] Display initializes (no blank/garbled screen — if it fails, try I2C
      address `0x3D` instead of `0x3C`).
- [ ] Each state's animation is visibly different and actually animates
      (eyes blink, mouth moves, etc.) — not five static frames.
- [ ] Animation keeps running smoothly with no stutter (confirms the
      `millis()`-based non-blocking approach is working).

## 2. WiFi, standalone

Confirm connect + `Serial` prints IP address. Kill the WiFi router or move
the board out of range mid-run and confirm `wifi_manager` detects the drop
and retries, and that `pet_face` shows `ERROR` while disconnected.

## 3. Microphone, standalone

Record a clip on button press, dump raw PCM stats over `Serial` (e.g.
min/max sample values) to confirm audio is actually being captured and
isn't silence/noise from a wiring mistake. A quick way to verify without
STT yet: log the buffer's peak amplitude and confirm it visibly jumps when
you speak near the mic.

## 4. Server, standalone (on the target machine with Python)

- [ ] `uvicorn app:app --host 0.0.0.0 --port 8000` starts without error,
      and fails loudly with a clear message if `GEMINI_API_KEY` or
      `GOOGLE_APPLICATION_CREDENTIALS` is missing (per `AGENTS.md` §7).
- [ ] `GET /health` from a browser/`curl` on another device on the same
      WiFi returns `{"status":"ok"}`.
- [ ] `POST /talk` with a short pre-recorded Malayalam WAV clip (`curl -F
      audio=@test.wav http://<ip>:8000/talk --output reply.wav`) returns a
      playable WAV file, and the reply is genuinely in Malayalam.
- [ ] Temporarily break `GEMINI_API_KEY` (wrong value) and confirm `/talk`
      returns a clean HTTP error, not a hang or a raw traceback.

## 5. Amp/speaker, standalone

Play a known test WAV (bundled or hardcoded short beep) through
`audio_playback` without going through the network path yet, confirming
I2S wiring to the MAX98357A is correct before adding the server dependency.

## 6. Full end-to-end

With all of the above passing individually:
- [ ] Press button → `LISTENING` animation → speak → `THINKING` animation →
      `TALKING` animation with audible Malayalam reply → back to `IDLE`.
- [ ] Total round-trip time is in the few-seconds range described in
      `docs/API.md`'s latency budget, not tens of seconds (if much slower,
      check WiFi signal and server machine load first).

## Failure-mode tests (must all recover cleanly, not hang/crash)

- [ ] Server not running at all → firmware shows `ERROR`, then `IDLE`,
      doesn't hang.
- [ ] WiFi disconnected mid-recording → same.
- [ ] Silence/no speech recorded → server's empty-transcript path
      (`docs/SERVER.md`) triggers, firmware doesn't try to play zero bytes.
- [ ] Gemini API key invalid/rate-limited → server returns clean error,
      firmware shows `ERROR`, then `IDLE`.
- [ ] Button held down past `MAX_RECORDING_MS` → recording is capped, not
      unbounded.
- [ ] Server machine's local IP changes (e.g. after router reboot) →
      firmware fails predictably with `ERROR` (expected — this requires a
      `config.h` update or a static IP reservation; document this as a
      known operational limitation in `README.md`, not a bug to code around
      in v1).

## Sign-off

The project is considered "working without fail" for v1 once every checkbox
above is ticked at least once on real hardware, and the full end-to-end
flow has been run repeatedly (10+ interactions) without a hang requiring a
manual power-cycle.
