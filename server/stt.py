"""stt.py — Google Cloud Speech-to-Text wrapper (ml-IN). See docs/API.md.

Contract (docs/SERVER.md):
  - input: raw PCM bytes, 16 kHz / 16-bit / mono (matches firmware capture)
  - returns "" when nothing is recognized rather than raising — app.py
    decides what to do with an empty transcript
  - genuine API/transport failures raise RuntimeError so app.py can turn
    them into a clean HTTP error
"""
from google.cloud import speech

_client: speech.SpeechClient | None = None


def _get_client() -> speech.SpeechClient:
    """Create the SpeechClient lazily so app.py's startup env-var validation
    runs before the Google client library reads the credentials file."""
    global _client
    if _client is None:
        _client = speech.SpeechClient()
    return _client


def transcribe_audio(audio_bytes: bytes) -> str:
    """Transcribe 16kHz/16-bit/mono LINEAR16 PCM to Malayalam text."""
    audio = speech.RecognitionAudio(content=audio_bytes)
    config = speech.RecognitionConfig(
        encoding=speech.RecognitionConfig.AudioEncoding.LINEAR16,
        sample_rate_hertz=16000,
        language_code="ml-IN",
    )
    try:
        response = _get_client().recognize(config=config, audio=audio)
    except Exception as exc:
        raise RuntimeError(f"speech-to-text request failed: {exc}") from exc

    if not response.results:
        return ""
    return response.results[0].alternatives[0].transcript
