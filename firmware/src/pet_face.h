#pragma once
// pet_face.h — mandatory OLED animation state machine (AGENTS.md §4).
// This module owns ALL pixel drawing. main.cpp only ever calls
// setState() / update() — never draws pixels directly.

#include <Arduino.h>

enum class PetState { IDLE, LISTENING, THINKING, TALKING, ERROR };

class PetFace {
public:
  bool begin();               // init I2C + SSD1306; false if display init fails
  void setState(PetState s);   // switch facial state
  PetState getState() const { return _state; }
  void update();               // call every loop() iteration; millis()-driven,
                               // never blocks, never uses delay()

private:
  void drawFrame(uint32_t now);
  void drawIdle(uint32_t now);
  void drawListening(uint32_t now);
  void drawThinking(uint32_t now);
  void drawTalking(uint32_t now);
  void drawError(uint32_t now);

  PetState _state = PetState::IDLE;
  uint32_t _lastFrameMs = 0;

  // IDLE animation scheduling
  uint32_t _nextBlinkMs = 0;
  uint32_t _blinkEndMs  = 0;
  int8_t   _lookX = 0;
  int8_t   _lookTarget = 0;
  uint32_t _nextLookMs = 0;
  uint32_t _lastLookStepMs = 0;

  bool _displayOk = false;
};

extern PetFace petFace;
