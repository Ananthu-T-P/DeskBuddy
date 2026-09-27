"""tts.py — Google Cloud Text-to-Speech wrapper (ml-IN). See docs/API.md.

Output is LINEAR16 at 16 kHz with a standard 44-byte WAV header, so the
firmware's playback code only ever deals with one format (docs/FIRMWARE.md).
Standard pre-built ml-IN voices are used — no voice cloning/training.

Raises RuntimeError on any API/transport failure so app.py can map it to a
clean HTTP error.
"""
from google.cloud import texttospeech

_client: texttospeech.TextToSpeechClient | None = None


def _get_client() -> texttospeech.TextToSpeechClient:
    """Created lazily so app.py's startup env-var validation runs first."""
    global _client
    if _client is None:
        _client = texttospeech.TextToSpeechClient()
    return _client


def synthesize_speech(text: str) -> bytes:
    """Malayalam text -> WAV bytes (LINEAR16, 16 kHz, 44-byte header)."""
    input_text = texttospeech.SynthesisInput(text=text)
    voice = texttospeech.VoiceSelectionParams(
        language_code="ml-IN",
        ssml_gender=texttospeech.SsmlVoiceGender.FEMALE,
    )
    audio_config = texttospeech.AudioConfig(
        audio_encoding=texttospeech.AudioEncoding.LINEAR16,
        sample_rate_hertz=16000,
    )
    try:
        response = _get_client().synthesize_speech(
            input=input_text, voice=voice, audio_config=audio_config
        )
    except Exception as exc:
        raise RuntimeError(f"text-to-speech request failed: {exc}") from exc

    if not response.audio_content:
        raise RuntimeError("text-to-speech returned empty audio")
    return response.audio_content
