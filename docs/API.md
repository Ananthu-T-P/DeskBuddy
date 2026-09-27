# docs/API.md — Gemini & Google Cloud Integration Spec

## Model/provider decision (fixed — do not change)

- **LLM: Google Gemini API only**, via the `google-generativeai` Python
  package. No other model provider is used anywhere in this project.
- **STT/TTS: Google Cloud Speech-to-Text and Text-to-Speech**, both set to
  `ml-IN` (Malayalam, India). These are a separate Google product from
  Gemini and need their own credentials (a service-account JSON key), even
  though both are "Google APIs" — don't assume one key covers both.

## Getting credentials

1. **Gemini API key** — from Google AI Studio (aistudio.google.com). This is
   a simple API key, goes in `GEMINI_API_KEY`.
2. **Google Cloud service account** — create a project in Google Cloud
   Console, enable "Cloud Speech-to-Text API" and "Cloud Text-to-Speech
   API", create a service account, download its JSON key, save as
   `server/gcloud-key.json`, and point
   `GOOGLE_APPLICATION_CREDENTIALS=gcloud-key.json` at it in `.env`.

Both have usable free tiers for hobby-level usage — see `README.md` for the
cost summary. Confirm current limits on Google's pricing pages before
assuming a specific number, since these change.

## `llm.py` — Gemini call spec

```python
import os
import google.generativeai as genai

genai.configure(api_key=os.environ["GEMINI_API_KEY"])
model = genai.GenerativeModel("gemini-2.0-flash")

SYSTEM_PROMPT = (
    "You are a cheerful desktop pet companion. Always reply only in "
    "Malayalam, using natural conversational Malayalam (not overly formal). "
    "Keep every reply to 1-2 short sentences, since it will be spoken aloud "
    "through a small speaker. Do not use English words unless there's no "
    "natural Malayalam equivalent. Do not include any stage directions, "
    "emoji, or formatting — plain spoken Malayalam text only."
)

def get_ai_reply(user_text: str) -> str:
    prompt = f"{SYSTEM_PROMPT}\n\nUser said: {user_text}\nYour reply:"
    response = model.generate_content(prompt)
    return response.text.strip()
```

Notes:
- `gemini-2.0-flash` (or the current fast/cheap Gemini model at build time)
  is the right choice here — this is a low-latency voice interaction, not a
  reasoning-heavy task, and flash-tier models keep round-trip time low and
  stay comfortably within free-tier limits.
- Wrap the call in try/except in `llm.py` or let `app.py`'s top-level
  handler catch it — either way, a Gemini error (rate limit, network) must
  become a clean HTTP error response to the ESP32, per `docs/SERVER.md`,
  not an unhandled exception.
- If Gemini ever replies with English or mixed language despite the system
  prompt, that's a prompt-engineering problem to iterate on (strengthen the
  instruction, e.g. add "Respond ONLY in Malayalam script, never in
  English") rather than a code bug — note this as a known tuning point.

## `stt.py` — Speech-to-Text call spec

```python
from google.cloud import speech

client = speech.SpeechClient()

def transcribe_audio(audio_bytes: bytes) -> str:
    audio = speech.RecognitionAudio(content=audio_bytes)
    config = speech.RecognitionConfig(
        encoding=speech.RecognitionConfig.AudioEncoding.LINEAR16,
        sample_rate_hertz=16000,
        language_code="ml-IN",
    )
    response = client.recognize(config=config, audio=audio)
    if not response.results:
        return ""
    return response.results[0].alternatives[0].transcript
```

Must match the firmware's recording format exactly (16kHz/16-bit/mono PCM —
see `docs/FIRMWARE.md`) or transcription quality will suffer badly.

## `tts.py` — Text-to-Speech call spec

```python
from google.cloud import texttospeech

client = texttospeech.TextToSpeechClient()

def synthesize_speech(text: str) -> bytes:
    input_text = texttospeech.SynthesisInput(text=text)
    voice = texttospeech.VoiceSelectionParams(
        language_code="ml-IN",
        ssml_gender=texttospeech.SsmlVoiceGender.FEMALE,
    )
    audio_config = texttospeech.AudioConfig(
        audio_encoding=texttospeech.AudioEncoding.LINEAR16,
        sample_rate_hertz=16000,
    )
    response = client.synthesize_speech(
        input=input_text, voice=voice, audio_config=audio_config
    )
    return response.audio_content
```

Standard pre-built `ml-IN` voices are sufficient and are what this project
uses — no voice cloning, no custom voice training. If a specific voice name
is wanted later, list available `ml-IN` voices via
`client.list_voices(language_code="ml-IN")` and pick one by name instead of
`ssml_gender`; that's an optional enhancement, not required for v1.

## Latency budget (rough)

STT (~0.5–1.5s) + Gemini (~0.5–2s) + TTS (~0.5–1.5s) + network overhead ≈
2–6 seconds total for `/talk` to respond. The `THINKING` OLED animation
(§4 in `AGENTS.md`) exists specifically to make this wait feel intentional
rather than broken — don't try to "optimize away" the THINKING state even
if latency improves.
