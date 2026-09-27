# docs/SERVER.md — Relay Server Spec

Python 3.10+, FastAPI + uvicorn. Runs on a laptop/Raspberry Pi on the same
WiFi network as the ESP32 — not on this development machine, which may not
have Python installed. A single free Gemini API key powers the whole
pipeline (docs/API.md); there is no Google Cloud service account.

## Endpoints

### `GET /health`

Returns `{"status": "ok"}`. Used by setup steps and by anyone debugging
connectivity from a phone/browser before wiring up the ESP32.

### `POST /talk`

- Accepts `multipart/form-data` with a single field `audio`: raw PCM bytes,
  16kHz / 16-bit / mono (see `docs/FIRMWARE.md` recording format).
- Pipeline: `llm.py` → `tts.py` (see `docs/API.md` for each):
  1. `llm.understand_and_reply(pcm)` — Gemini hears the audio and writes a
     short Malayalam reply (or the `NO_SPEECH` sentinel).
  2. `tts.synthesize_speech(reply)` — Gemini TTS renders it as 16 kHz WAV.
- Returns: `audio/wav` bytes of the Malayalam reply on success.
- On any internal failure (Gemini error, TTS error, network), return an
  HTTP error status (e.g. 502) with a small JSON body describing the
  failure — never let an unhandled exception produce a bare 500 with a
  stack trace as the body; the firmware only needs to know
  "succeeded → play audio" or "failed → show ERROR state", but clear
  server-side logging (`print`/`logging`) of the real error is required for
  debugging.

## File responsibilities

- **`app.py`** — FastAPI app, the two routes above, startup validation of
  the required env var (`GEMINI_API_KEY`), and top-level try/except around
  the `/talk` pipeline that converts any exception into a clean error
  response.
- **`llm.py`** — Gemini audio understanding + Malayalam reply. Wraps the PCM
  in a WAV header, sends it with the pet-personality system prompt (docs/API.md),
  and returns reply text. Returns the `NO_SPEECH` sentinel verbatim when the
  clip has no clear speech — `app.py` decides what to do with that.
- **`tts.py`** — Gemini Text-to-Speech. Malayalam text in, 16 kHz
  LINEAR16 WAV out (resamples Gemini's 24 kHz PCM and adds the WAV header)
  so the firmware's playback code only ever deals with one format.

## Environment / secrets

`server/.env` is tracked WITH A PLACEHOLDER (owner's decision), so anyone
cloning the repo sees the complete config surface:

```
GEMINI_API_KEY=paste_your_gemini_api_key_here
```

Fill in the real key ONLY on the machine that runs the server, and never
commit/push that edit back (AGENTS.md §8). That one variable is the entire
credential surface of the project.

## Running it

```bash
cd server
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
# edit .env (tracked in the repo) — paste your real Gemini key
uvicorn app:app --host 0.0.0.0 --port 8000
```

`--host 0.0.0.0` is required — `127.0.0.1` would refuse connections from the
ESP32. If the ESP32 can't reach the server, check the host machine's
firewall allows inbound connections on port 8000, and that both devices are
on the same WiFi network (see `README.md` troubleshooting pointer to
`docs/TESTING.md`).

## No-speech handling

If `llm.py` returns the `NO_SPEECH` sentinel (silence/noise in the clip),
`app.py` skips the reply path and synthesizes a short canned Malayalam
"I didn't catch that" line via `tts.py` — approach (a) from the original
spec: a small *generated* Malayalam clip, so the pet gives friendly audio
feedback instead of an error buzz. If that fallback TTS call also fails,
`/talk` returns the standard 502 JSON error. This decision is also
documented in a comment at the top of `app.py`.
