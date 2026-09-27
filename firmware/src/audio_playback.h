#pragma once
// audio_playback.h — MAX98357A I2S playback of the server's WAV reply.
// Non-blocking: play() copies the clip, update() streams it chunk by chunk.

#include <Arduino.h>

class AudioPlayback {
public:
  bool begin();   // install + start the amp I2S peripheral

  // Accepts a WAV buffer (44-byte RIFF header is skipped, docs/FIRMWARE.md)
  // or raw 16kHz/16-bit mono PCM. Fails cleanly on empty/oversized input.
  bool play(const uint8_t* wavBytes, size_t len);

  void update();                      // stream next chunk per call
  bool isPlaying() const { return _playing; }

  // Progress reporting so callers can reason about playback length
  // (docs/FIRMWARE.md — pet_face drives TALKING on its own timer).
  uint32_t durationMs() const { return _durationMs; }
  uint32_t elapsedMs() const { return _playing ? millis() - _startMs : 0; }

  void stop();                        // cut playback + free the buffer

private:
  uint8_t* _pcm = nullptr;
  size_t   _len = 0;
  size_t   _offset = 0;
  uint32_t _startMs = 0;
  uint32_t _durationMs = 0;
  bool     _playing = false;
  bool     _installed = false;  // I2S driver up? guards every port access
};

extern AudioPlayback audioPlayback;
