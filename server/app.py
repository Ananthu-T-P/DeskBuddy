"""app.py — FastAPI relay server for the ESP32 desktop pet (docs/SERVER.md).

All-Gemini design (AGENTS.md §2.2): ONE free Gemini API key does all three
jobs — hearing (audio understanding), thinking (LLM reply), and speaking
(Gemini TTS). No Google Cloud project, service account, or billing file.

Routes:
  GET  /health -> {"status": "ok"}
  POST /talk   -> multipart/form-data, single field "audio" (raw PCM,
                  16 kHz / 16-bit / mono) IN, audio/wav bytes OUT.
                  Pipeline: llm.py (audio -> Malayalam reply text)
                            -> tts.py (reply -> 16 kHz WAV).

No-speech policy (docs/SERVER.md, empty-transcript handling): approach (a) —
when Gemini hears no intelligible speech it returns the NO_SPEECH sentinel;
we synthesize a short generated Malayalam "I didn't catch that" clip via
tts.py instead of treating it as an error. If that TTS call also fails,
/talk falls through to the generic 502 handler.

Failure contract (AGENTS.md §7 / docs/SERVER.md):
  any pipeline failure -> HTTP 502 with a small JSON body describing it —
  never a bare 500 with a stack trace; the firmware only needs
  "200 + audio => play it" vs "anything else => ERROR face".

Runs on a machine with Python 3.10+. Start with:
  uvicorn app:app --host 0.0.0.0 --port 8000
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

REQUIRED_ENV_VARS = ("GEMINI_API_KEY",)
_PLACEHOLDER_VALUES = {"", "paste_your_gemini_api_key_here"}


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
            + "\nEdit server/.env and paste your free Gemini API key in "
              "place of the placeholder (https://aistudio.google.com — see "
              "docs/API.md)."
        )


_validate_startup_config()

# Imported only after config validation: both read GEMINI_API_KEY at import.
import llm  # noqa: E402
import tts  # noqa: E402

app = FastAPI(title="Desktop Pet Relay", version="1.1.0")


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

        reply_text = llm.understand_and_reply(pcm)
        if reply_text == llm.NO_SPEECH:
            log.info("/talk: no clear speech -> Malayalam fallback clip")
            reply_audio = tts.synthesize_speech(DIDNT_CATCH_THAT)
        else:
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
