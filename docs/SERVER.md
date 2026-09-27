# docs/SERVER.md — Relay Server Spec

Python 3.10+, FastAPI + uvicorn. Runs on a laptop/Raspberry Pi on the same
WiFi network as the ESP32 — not on this development machine, which may not
have Python installed. Write and review this code carefully since it can't
be run here; keep functions small and each one's contract obvious so
mistakes are easy to catch by reading.

## Endpoints

### `GET /health`

Returns `{"status": "ok"}`. Used by setup steps and by anyone debugging
connectivity from a phone/browser before wiring up the ESP32.

### `POST /talk`

- Accepts `multipart/form-data` with a single field `audio`: raw PCM bytes,
  16kHz / 16-bit / mono (see `docs/FIRMWARE.md` recording format).
- Pipeline: `stt.py` → `llm.py` → `tts.py` (see `docs/API.md` for each).
- Returns: `audio/wav` bytes of the Malayalam reply on success.
- On any internal failure (STT empty, Gemini error, TTS error), return an
  HTTP error status (e.g. 502) with a small JSON body describing the
  failure — never let an unhandled exception produce a bare 500 with a
  stack trace as the body; the firmware only needs to know
  "succeeded → play audio" or "failed → show ERROR state", but clear
  server-side logging (`print`/`logging`) of the real error is required for
  debugging.

## File responsibilities

- **`app.py`** — FastAPI app, the two routes above, startup validation of
  required env vars (`GEMINI_API_KEY`, `GOOGLE_APPLICATION_CREDENTIALS`),
  and top-level try/except around the `/talk` pipeline that converts any
  exception into a clean error response.
- **`stt.py`** — wraps Google Cloud Speech-to-Text, `language_code="ml-IN"`,
  `encoding=LINEAR16`, `sample_rate_hertz=16000`. Returns `""` (empty
  string) if there's no result, rather than raising — `app.py` decides what
  to do with an empty transcript.
- **`llm.py`** — wraps the Gemini API. System prompt must explicitly pin
  the reply language to Malayalam and keep replies short (1–2 sentences,
  since they get spoken aloud through a small speaker). See `docs/API.md`
  for the exact prompt and model name to use.
- **`tts.py`** — wraps Google Cloud Text-to-Speech, `language_code="ml-IN"`,
  `LINEAR16` output at 16kHz so the firmware's playback code doesn't need
  to handle multiple sample rates.

## Environment / secrets

`server/.env.example` (real `.env` is gitignored):

```
GEMINI_API_KEY=your_gemini_key_here
GOOGLE_APPLICATION_CREDENTIALS=gcloud-key.json
```

`gcloud-key.json` (the downloaded service-account key) is also gitignored
and must never be committed.

## Running it

```bash
cd server
python3 -m venv venv
source venv/bin/activate
pip install -r requirements.txt
cp .env.example .env   # fill in real values
uvicorn app:app --host 0.0.0.0 --port 8000
```

`--host 0.0.0.0` is required — `127.0.0.1` would refuse connections from the
ESP32. If the ESP32 can't reach the server, check the host machine's
firewall allows inbound connections on port 8000, and that both devices are
on the same WiFi network (see `README.md` troubleshooting pointer to
`docs/TESTING.md`).

## Empty-transcript handling

If `stt.py` returns `""`, `app.py` should skip calling Gemini/TTS entirely
and either (a) return a small pre-recorded/generated "I didn't catch that"
Malayalam audio clip, or (b) return a defined lightweight error the
firmware treats as "go to ERROR briefly, then IDLE" rather than trying to
play zero bytes of audio. Pick one approach and document which one you
implemented directly in a comment at the top of `app.py`.
