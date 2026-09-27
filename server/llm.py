"""llm.py — Gemini audio understanding + Malayalam pet reply (docs/API.md).

Gemini is the ONLY AI service in this project (AGENTS.md §2.2): it hears the
raw clip from the ESP32 (audio input — there is no separate speech-to-text
service) and writes a short Malayalam reply.

If the clip contains no clear speech (silence/noise), the model returns the
NO_SPEECH sentinel and app.py plays the canned Malayalam fallback clip
instead. Raises RuntimeError on any Gemini failure so app.py can map it to
a clean HTTP error (AGENTS.md §7).
"""
import io
import logging
import os
import wave

import numpy as np
from google import genai
from google.genai import types

log = logging.getLogger("desktop-pet.llm")

# Fast/cheap flash-tier model — this is a low-latency voice interaction.
# If Google has renamed it, use the current flash model name from
# https://ai.google.dev/gemini-api/docs/models
MODEL = "gemini-2.5-flash"

# Sentinel the model must return verbatim when the clip has no clear speech.
NO_SPEECH = "NO_SPEECH"

_client = genai.Client(api_key=os.environ["GEMINI_API_KEY"])

SYSTEM_PROMPT = (
    "You are a cheerful desktop pet companion. Always reply only in "
    "Malayalam, using natural conversational Malayalam (not overly formal). "
    "Keep every reply to 1-2 short sentences, since it will be spoken aloud "
    "through a small speaker. Do not use English words unless there's no "
    "natural Malayalam equivalent. Do not include any stage directions, "
    "emoji, or formatting — plain spoken Malayalam text only."
)

_AUDIO_INSTRUCTION = (
    SYSTEM_PROMPT
    + "\n\nThe user is talking to you in the attached audio clip. The clip "
      "was recorded on a small desk microphone and may contain background "
      "noise (fan, TV, people talking in another room) — focus on the "
      "clearest human voice and ignore the noise. If the clip is silence or "
      "has no understandable speech at all, reply with exactly: "
    + NO_SPEECH
    + "\nOtherwise, understand what they said (it may be Malayalam or "
      "another language) and give your reply following the rules above."
)


def _pcm_to_wav(pcm_bytes: bytes) -> bytes:
    """Wrap the firmware's raw 16 kHz/16-bit/mono PCM in a WAV header so the
    Gemini request uses the unambiguous `audio/wav` MIME type."""
    buf = io.BytesIO()
    with wave.open(buf, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)  # 16-bit
        w.setframerate(16000)
        w.writeframes(pcm_bytes)
    return buf.getvalue()


def _normalize_pcm(pcm_bytes: bytes) -> bytes:
    """Boost quiet recordings toward full scale so speech stays intelligible
    over room noise (the INMP441 is a few inches from the speaker, raw level
    is often low). Gain is capped at 8x so heavy noise isn't over-amplified,
    and near-silent clips are passed through untouched so the NO_SPEECH
    sentinel path still triggers."""
    samples = np.frombuffer(pcm_bytes, dtype=np.int16)
    if samples.size == 0:
        return pcm_bytes
    peak = int(np.max(np.abs(samples.astype(np.int32))))
    if peak < 500:      # effectively silence — don't amplify the noise floor
        return pcm_bytes
    gain = 32767.0 / peak
    if gain <= 1.1:
        return pcm_bytes  # already loud enough
    gain = min(gain, 8.0)
    boosted = np.clip(samples.astype(np.float32) * gain, -32768, 32767)
    return boosted.astype(np.int16).tobytes()


def understand_and_reply(pcm_bytes: bytes) -> str:
    """16 kHz/16-bit/mono PCM clip -> short Malayalam reply text.

    Returns the NO_SPEECH sentinel when nothing intelligible was said.
    """
    try:
        response = _client.models.generate_content(
            model=MODEL,
            contents=[
                _AUDIO_INSTRUCTION,
                types.Part.from_bytes(
                    data=_pcm_to_wav(_normalize_pcm(pcm_bytes)),
                    mime_type="audio/wav",
                ),
            ],
        )
        text = (response.text or "").strip()
    except Exception as exc:
        raise RuntimeError(f"gemini audio request failed: {exc}") from exc

    if not text:
        raise RuntimeError("gemini returned an empty reply")
    if text == NO_SPEECH:
        log.info("gemini heard no clear speech in the clip")
    return text
