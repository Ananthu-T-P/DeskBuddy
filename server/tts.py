"""tts.py — Gemini Text-to-Speech: Malayalam text -> 16 kHz WAV (docs/API.md).

Malayalam is a supported Gemini TTS language. The TTS model emits raw PCM at
24 kHz; we resample to 16 kHz (the firmware's fixed playback rate,
docs/FIRMWARE.md) and wrap it in a standard WAV header, so the server->
firmware contract never changes.

Raises RuntimeError on any Gemini failure so app.py can map it to a clean
HTTP error (AGENTS.md §7).
"""
import io
import logging
import os
import wave

import numpy as np
from google import genai
from google.genai import types

log = logging.getLogger("desktop-pet.tts")

# Fast Gemini TTS model. If Google renames it, check the current name:
# https://ai.google.dev/gemini-api/docs/speech-generation
TTS_MODEL = "gemini-2.5-flash-preview-tts"
VOICE_NAME = "Kore"  # any prebuilt Gemini voice works — see speech docs

GEMINI_TTS_RATE = 24000  # Gemini TTS output is 24 kHz 16-bit mono PCM
TARGET_RATE = 16000      # the firmware only ever plays 16 kHz

_client = genai.Client(api_key=os.environ["GEMINI_API_KEY"])


def _to_16khz_wav(pcm24: bytes) -> bytes:
    """Gemini 24 kHz raw PCM -> 16 kHz RIFF/WAV the firmware plays directly."""
    samples = np.frombuffer(pcm24, dtype=np.int16)
    if samples.size == 0:
        raise RuntimeError("gemini TTS returned empty audio")

    n_out = int(round(samples.size * TARGET_RATE / GEMINI_TTS_RATE))
    old_x = np.linspace(0.0, samples.size - 1, num=samples.size)
    new_x = np.linspace(0.0, samples.size - 1, num=n_out)
    resampled = np.interp(new_x, old_x, samples.astype(np.float32))
    pcm16 = np.clip(resampled, -32768, 32767).astype(np.int16).tobytes()

    out = io.BytesIO()
    with wave.open(out, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(TARGET_RATE)
        w.writeframes(pcm16)
    return out.getvalue()


def synthesize_speech(text: str) -> bytes:
    """Malayalam text -> WAV bytes (LINEAR16, 16 kHz, standard header)."""
    try:
        response = _client.models.generate_content(
            model=TTS_MODEL,
            contents=(
                "Say the following Malayalam text aloud in a warm, cheerful "
                "voice, speaking naturally:\n" + text
            ),
            config=types.GenerateContentConfig(
                response_modalities=["AUDIO"],
                # The TTS model auto-detects the language from the text
                # (Malayalam here). Newer SDK versions also accept
                # language_code="ml-IN" inside SpeechConfig.
                speech_config=types.SpeechConfig(
                    voice_config=types.VoiceConfig(
                        prebuilt_voice_config=types.PrebuiltVoiceConfig(
                            voice_name=VOICE_NAME
                        )
                    )
                ),
            ),
        )
        pcm24 = response.candidates[0].content.parts[0].inline_data.data
    except Exception as exc:
        raise RuntimeError(f"gemini TTS request failed: {exc}") from exc

    if not pcm24:
        raise RuntimeError("gemini TTS returned empty audio")
    return _to_16khz_wav(pcm24)
