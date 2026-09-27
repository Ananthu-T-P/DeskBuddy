"""app.py — FastAPI relay server for the ESP32 desktop pet (docs/SERVER.md).

Routes:
  GET  /health -> {"status": "ok"}
  POST /talk   -> multipart/form-data, single field "audio" (raw PCM,
                  16 kHz / 16-bit / mono) IN, audio/wav bytes OUT.
                  Pipeline: stt.py -> llm.py -> tts.py (docs/API.md).

Empty-transcript policy (docs/SERVER.md, "Empty-transcript handling"):
  approach (a) is implemented — when STT returns "" (silence/noise), Gemini
  is skipped entirely and a short *generated* Malayalam "I didn't catch
  that" clip is returned via tts.py. If that TTS call also fails, /talk
  falls through to the generic 502 handler.

Failure contract (AGENTS.md §7 / docs/SERVER.md):
  any pipeline failure -> HTTP 502 with a small JSON body describing it —
  never a bare 500 with a stack trace; the firmware only needs
  "200 + audio => play it" vs "anything else => ERROR face".

Runs on a machine with Python 3.10+ — not the machine this code was
written on. Start with: uvicorn app:app --host 0.0.0.0 --port 8000
"""
import logging
import os

from dotenv import load_dotenv
from fastapi import FastAPI, File, HTTPException, UploadFile
from fastapi.responses import JSONResponse, Response

load_dotenv()

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s %(name)s: %(message)s",
)
log = logging.getLogger("desktop-pet")

# Short Malayalam fallback played when nothing is understood:
# "Sorry, I didn't understand."
DIDNT_CATCH_THAT = "ക്ഷമിക്കണം, എനിക്ക് മനസ്സിലായില്ല."

REQUIRED_ENV_VARS = ("GEMINI_API_KEY", "GOOGLE_APPLICATION_CREDENTIALS")
_PLACEHOLDER_VALUES = {"", "your_gemini_key_here"}


def _validate_startup_config() -> None:
    """Refuse to start with missing/placeholder config (AGENTS.md §7) —
    fail loudly at boot, not on the first /talk request."""
    missing = [
        name
        for name in REQUIRED_ENV_VARS
        if os.environ.get(name, "").strip() in _PLACEHOLDER_VALUES
    ]
    if missing:
        raise SystemExit(
            "CONFIG ERROR: missing/placeholder env var(s): "
            + ", ".join(missing)
            + "\nCopy .env.example to .env and fill in real values "
              "(see docs/API.md for how to get the keys)."
        )
    creds = os.environ["GOOGLE_APPLICATION_CREDENTIALS"]
    if not os.path.isfile(creds):
        raise SystemExit(
            f"CONFIG ERROR: GOOGLE_APPLICATION_CREDENTIALS points to "
            f"'{creds}', but that file does not exist (relative to the "
            f"directory you start uvicorn from). Download the service-account "
            f"JSON key per docs/API.md and place it next to app.py."
        )


_validate_startup_config()

# Imported only after config validation: llm.py reads GEMINI_API_KEY at
# import time, and the Google STT/TTS clients should only be needed in a
# correctly-configured environment.
import llm  # noqa: E402
import stt  # noqa: E402
import tts  # noqa: E402

app = FastAPI(title="Desktop Pet Relay", version="1.0.0")


@app.get("/health")
def health() -> dict:
    return {"status": "ok"}


@app.post("/talk")
async def talk(audio: UploadFile = File(...)) -> Response:
    try:
        pcm = await audio.read()
        if not pcm:
            # Client sent an empty "audio" field — that's a client bug.
            raise HTTPException(status_code=400, detail="empty audio field")
        log.info("/talk: received %d bytes of PCM", len(pcm))

        transcript = stt.transcribe_audio(pcm)
        if not transcript.strip():
            log.info("/talk: empty transcript -> Malayalam fallback clip")
            reply_audio = tts.synthesize_speech(DIDNT_CATCH_THAT)
            return Response(content=reply_audio, media_type="audio/wav")

        log.info("/talk: transcript: %s", transcript)
        reply_text = llm.get_ai_reply(transcript)
        log.info("/talk: gemini reply: %s", reply_text)

        reply_audio = tts.synthesize_speech(reply_text)
        log.info("/talk: replying with %d bytes of WAV", len(reply_audio))
        return Response(content=reply_audio, media_type="audio/wav")

    except HTTPException:
        raise
    except Exception as exc:  # noqa: BLE001 — the failure boundary is deliberate
        log.exception("/talk pipeline failed")
        return JSONResponse(
            status_code=502,
            content={"status": "error", "detail": str(exc)[:300]},
        )
