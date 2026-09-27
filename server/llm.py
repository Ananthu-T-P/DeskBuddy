"""llm.py — Gemini API wrapper. Replies are pinned to Malayalam (docs/API.md).

Gemini is the only LLM in this project (AGENTS.md §2.2). Fast/cheap flash
model: this is a low-latency voice interaction, not a reasoning task.

Note (docs/API.md): if Gemini ever replies in English despite the system
prompt, that is a prompt-tuning problem (strengthen the instruction), not a
code bug.
"""
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
    """User transcript -> short Malayalam pet reply.

    Raises RuntimeError on any Gemini failure (rate limit, network, safety
    block, empty reply) so app.py can map it to a clean HTTP error.
    """
    prompt = f"{SYSTEM_PROMPT}\n\nUser said: {user_text}\nYour reply:"
    try:
        response = model.generate_content(prompt)
        text = response.text.strip()
    except Exception as exc:
        raise RuntimeError(f"gemini request failed: {exc}") from exc
    if not text:
        raise RuntimeError("gemini returned an empty reply")
    return text
