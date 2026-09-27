// pet_face.cpp — every animation frame lives here (AGENTS.md §4).
//
// Rendered states (all ANIMATED, all driven by millis(), no delay()):
//   IDLE      : friendly rounded eyes, randomized 3–5 s blinks, slow look-around
//   LISTENING : widened eyes + animated waveform bars under the eyes
//   THINKING  : narrowed (squinting) eyes + rotating-dot "loading" wheel
//   TALKING   : mouth alternates open/closed on a timer (no true lip-sync)
//   ERROR     : X X eyes + wavy mouth, gently wobbling so it's clearly alive
//
// Standalone test (docs/TESTING.md §1): flash with only the OLED wired and
// cycle setState() through all five states on a timer.

#include "pet_face.h"
#include "pins.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <esp_system.h>  // esp_random()

PetFace petFace;

// The display object is private to this module.
static Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

namespace {
constexpr uint32_t FRAME_MS      = 33;   // ~30 fps redraw cap
constexpr int      EYE_LX        = 40;   // left eye centre X
constexpr int      EYE_RX        = 88;   // right eye centre X
constexpr int      EYE_Y         = 26;
constexpr int      EYE_R         = 10;   // resting eye radius
constexpr uint32_t BLINK_HOLD_MS = 140;
constexpr int      MOUTH_Y       = 50;

void drawOpenEye(int cx, int cy, int r) {
  display.fillCircle(cx, cy, r, SSD1306_WHITE);
}

void drawClosedEye(int cx, int cy, int r) {
  display.fillRoundRect(cx - r, cy - 1, 2 * r, 3, 1, SSD1306_WHITE);
}

void drawXEye(int cx, int cy, int r) {
  // drawn twice, 1 px offset, for thickness
  for (int o = 0; o <= 1; o++) {
    display.drawLine(cx - r, cy - r + o, cx + r, cy + r + o, SSD1306_WHITE);
    display.drawLine(cx - r, cy + r - o, cx + r, cy - r - o, SSD1306_WHITE);
  }
}
}  // namespace

bool PetFace::begin() {
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDRESS)) {
    // Caller logs a Serial error; update() becomes a no-op so the rest of
    // the firmware keeps running instead of crashing.
    _displayOk = false;
    return false;
  }
  _displayOk = true;
  display.clearDisplay();
  display.display();

  randomSeed(esp_random());
  uint32_t now = millis();
  _nextBlinkMs = now + 1500;
  _nextLookMs  = now + 3000;
  return true;
}

void PetFace::setState(PetState s) {
  if (s == _state) return;
  _state = s;
  // Reset idle schedulers so a return to IDLE behaves naturally.
  uint32_t now = millis();
  _nextBlinkMs = now + 1200;
  _nextLookMs  = now + 2000;
}

void PetFace::update() {
  if (!_displayOk) return;
  uint32_t now = millis();
  if (now - _lastFrameMs < FRAME_MS) return;  // frame-rate cap, non-blocking
  _lastFrameMs = now;
  drawFrame(now);
}

void PetFace::drawFrame(uint32_t now) {
  display.clearDisplay();
  switch (_state) {
    case PetState::IDLE:      drawIdle(now);      break;
    case PetState::LISTENING: drawListening(now); break;
    case PetState::THINKING:  drawThinking(now);  break;
    case PetState::TALKING:   drawTalking(now);   break;
    case PetState::ERROR:     drawError(now);     break;
  }
  display.display();
}

// ---------------------------------------------------------------- IDLE
void PetFace::drawIdle(uint32_t now) {
  // Blink every 3–5 s, randomized.
  if (now >= _nextBlinkMs) {
    _blinkEndMs  = now + BLINK_HOLD_MS;
    _nextBlinkMs = now + 3000 + (uint32_t)random(0, 2000);
  }
  const bool blinking = now < _blinkEndMs;

  // Occasional slow look-around: pick a new gaze target every 2.5–4 s,
  // ease toward it one pixel at a time.
  if (now >= _nextLookMs) {
    _lookTarget = (int8_t)random(-4, 5);
    _nextLookMs = now + 2500 + (uint32_t)random(0, 1500);
  }
  if (_lookX != _lookTarget && now - _lastLookStepMs >= 60) {
    _lookX += (_lookTarget > _lookX) ? 1 : -1;
    _lastLookStepMs = now;
  }

  if (blinking) {
    drawClosedEye(EYE_LX + _lookX, EYE_Y, EYE_R);
    drawClosedEye(EYE_RX + _lookX, EYE_Y, EYE_R);
  } else {
    drawOpenEye(EYE_LX + _lookX, EYE_Y, EYE_R);
    drawOpenEye(EYE_RX + _lookX, EYE_Y, EYE_R);
  }

  // Small friendly smile that follows the gaze a little.
  const int mx = 64 + _lookX / 2;
  for (int dx = -6; dx <= 6; dx++) {
    display.drawPixel(mx + dx, MOUTH_Y + 5 - (dx * dx) / 12, SSD1306_WHITE);
  }
}

// ----------------------------------------------------------- LISTENING
void PetFace::drawListening(uint32_t now) {
  // Eyes widen slightly.
  drawOpenEye(EYE_LX, EYE_Y, EYE_R + 2);
  drawOpenEye(EYE_RX, EYE_Y, EYE_R + 2);

  // Animated waveform/pulse under the eyes while audio is captured.
  for (int i = 0; i < 5; i++) {
    int h = 4 + (int)(9.0f * fabsf(sinf(now / 110.0f + i * 0.9f)));
    int x = 44 + i * 10;
    display.fillRect(x, MOUTH_Y + 8 - h, 4, h, SSD1306_WHITE);
  }
}

// ------------------------------------------------------------ THINKING
void PetFace::drawThinking(uint32_t now) {
  // Eyes narrow slightly (squint).
  display.fillRoundRect(EYE_LX - EYE_R - 1, EYE_Y - 3, 2 * (EYE_R + 1), 7, 3, SSD1306_WHITE);
  display.fillRoundRect(EYE_RX - EYE_R - 1, EYE_Y - 3, 2 * (EYE_R + 1), 7, 3, SSD1306_WHITE);

  // Rotating-dot loading cue around the mouth area.
  const uint8_t lit = (uint8_t)((now / 140) % 8);
  for (uint8_t i = 0; i < 8; i++) {
    float a = i * (PI / 4.0f);
    int x = 64 + (int)(9.0f * cosf(a));
    int y = MOUTH_Y + (int)(9.0f * sinf(a));
    if (i == lit) display.fillCircle(x, y, 2, SSD1306_WHITE);
    else          display.drawCircle(x, y, 2, SSD1306_WHITE);
  }
}

// ------------------------------------------------------------- TALKING
void PetFace::drawTalking(uint32_t now) {
  drawOpenEye(EYE_LX, EYE_Y, EYE_R);
  drawOpenEye(EYE_RX, EYE_Y, EYE_R);

  // Timer-driven open/closed mouth (true lip-sync explicitly out of scope).
  const bool open = ((now / 180) % 2) == 0;
  if (open) display.fillRoundRect(52, MOUTH_Y - 6, 24, 14, 5, SSD1306_WHITE);
  else      display.fillRoundRect(52, MOUTH_Y - 1, 24, 5, 2, SSD1306_WHITE);
}

// -------------------------------------------------------------- ERROR
void PetFace::drawError(uint32_t now) {
  // Gentle horizontal wobble so the error face is visibly animated too.
  const int shake = (int)(1.5f * sinf(now / 90.0f));
  drawXEye(EYE_LX + shake, EYE_Y, 7);
  drawXEye(EYE_RX + shake, EYE_Y, 7);

  // Wavy "confused" mouth; the zigzag flips phase so it visibly moves.
  const int phase = (int)((now / 250) % 2);
  int prevX = 50;
  int prevY = MOUTH_Y + (phase == 0 ? -3 : 3);
  for (int x = 54, i = 1; x <= 78; x += 4, i++) {
    int y = MOUTH_Y + (((i + phase) % 2 == 0) ? -3 : 3);
    display.drawLine(prevX, prevY, x, y, SSD1306_WHITE);
    prevX = x;
    prevY = y;
  }
}
