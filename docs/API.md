# docs/API.md — Gemini Integration Spec (all-Gemini design)

## Provider decision (fixed — do not change)

**Everything runs on the Google Gemini API with one free API key** (AGENTS.md
§2.2):

| Capability | How | Model |
|---|---|---|
| Hearing | Gemini audio understanding: the recorded PCM clip goes straight into a multimodal — no separate Speech-to-Text service exists in this project | `gemini-2.5-flash` |
| Thinking | The same model writes the short Malayalam pet reply in the same call | `gemini-2.5-flash` |
| Speaking | Gemini TTS (Malayalam is a supported language) | `gemini-2.5-flash-preview-tts` |

Do not substitute OpenAI, Claude, or Google Cloud Speech-to-Text /
Text-to-Speech. The all-Gemini design deliberately removed Google Cloud: one
credential total, no service-account JSON, no Cloud project. (Earlier
revisions of this doc specified Google Cloud STT/TTS — that design is
obsolete.)

## Getting the credential

**Gemini API key** — from Google AI Studio (https://aistudio.google.com →
"Get API key"). The free tier is plenty for a desktop pet (hundreds of
requests/day available; a pet speaks dozens). Put it in `server/.env` as
`GEMINI_API_KEY=...` on the machine that runs the server, and nowhere else.

Note: Google sometimes asks *new* accounts to "enable billing" on the backing
project before the free tier activates. That stores a card but charges
nothing inside free-tier limits — set a ₹0/$0 budget alert if you want
certainty. Current free-tier limits move occasionally; check
https://ai.google.dev/gemini-api/docs/rate-limits if the pet is heavily used.

## `llm.py` — audio understanding + reply

Wrap the firmware's raw 16 kHz/16-bit/mono PCM in a WAV header and send it as
`audio/wav` inline data with a text instruction:

```python
from google import genai
from google.genai import types

client = genai.Client(api_key=os.environ["GEMINI_API_KEY"])
MODEL = "gemini-2.5-flash"   # or the current fast/cheap flash model

SYSTEM_PROMPT = (
    "You are a cheerful desktop pet companion. Always reply only in "
    "Malayalam, using natural conversational Malayalam (not overly formal). "
    "Keep every reply to 1-2 short sentences, since it will be spoken aloud "
    "through a small speaker. Do not use English words unless there's no "
    "natural Malayalam equivalent. Do not include any stage directions, "
    "emoji, or formatting — plain spoken Malayalam text only."
)

# The prompt also carries a sentinel rule: if the clip is silence/noise the
# model must reply with exactly "NO_SPEECH" — app.py then plays its canned
# Malayalam fallback clip and never calls TTS with empty text.
response = client.models.generate_content(
    model=MODEL,
    contents=[SYSTEM_PROMPT + sentinel_rule,
              types.Part.from_bytes(data=wav_bytes, mime_type="audio/wav")],
)
reply_text = response.text.strip()
```

Notes:
- **Noise robustness** (requirement): `llm.py` peak-normalizes the clip
  before sending (≤8× gain cap, near-silent clips untouched) and the prompt
  explicitly tells Gemini the audio comes from a small desk mic with
  background noise and to focus on the clearest human voice. You can
  strengthen noise handling further later (e.g. a `noisereduce`
  pre-pass) without touching the firmware or the HTTP contract.
- If Gemini ever replies in English despite the prompt, that is a
  prompt-tuning problem, not a code bug (strengthen the instruction).
- Wrap the call in try/except (`llm.py` raises `RuntimeError`) so `app.py`
  can return a clean HTTP 502 to the ESP32 — never a raw traceback
  (AGENTS.md §7).

## `tts.py` — Gemini TTS

```python
response = client.models.generate_content(
    model="gemini-2.5-flash-preview-tts",
    contents=f"Say the following Malayalam text aloud in a warm, cheerful "
             f"voice, speaking naturally:\n{text}",
    config=types.GenerateContentConfig(
        response_modalities=["AUDIO"],
        speech_config=types.SpeechConfig(
            voice_config=types.VoiceConfig(
                prebuilt_voice_config=types.PrebuiltVoiceConfig(
                    voice_name="Kore")))),
)
pcm24 = response.candidates[0].content.parts[0].inline_data.data
```

Hard requirements and quirks:
- **Output is 24 kHz PCM, not 16 kHz.** `tts.py` MUST resample to 16 kHz and
  re-wrap as a standard RIFF/WAV before returning — the firmware's playback
  rate is fixed at 16 kHz (`docs/FIRMWARE.md`) and skips a 44-byte WAV
  header. The server→firmware contract (16 kHz WAV) must never change.
- The TTS model auto-detects the language from the text; since the reply is
  Malayalam, Malayalam speech comes out. Newer SDK versions also accept
  `language_code="ml-IN"` in `SpeechConfig` — optional hardening.
- Prebuilt voices (`Kore`, `Aoede`, ...) are sufficient — no voice cloning.
- If Google renames the TTS model, only the `TTS_MODEL` constant changes;
  check https://ai.google.dev/gemini-api/docs/speech-generation.

## Latency budget (rough)

Gemini audio-understanding+reply (~1.5–4 s) + Gemini TTS (~1–2 s) + LAN
overhead ≈ **3–6 s total** for `/talk`. The `THINKING` OLED animation exists
specifically to make this wait feel intentional rather than broken — don't
try to "optimize away" the THINKING state even if latency improves.
