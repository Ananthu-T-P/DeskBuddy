#pragma once
// audio_capture.h — bounded, non-blocking INMP441 recording.
// Output format: 16 kHz / 16-bit signed little-endian / mono PCM,
// exactly what Google Speech-to-Text expects for LINEAR16 (docs/API.md).

#include <Arduino.h>

class AudioCapture {
public:
  bool begin();                        // install + start the mic I2S peripheral
  bool startRecording(uint32_t maxMs); // allocate buffer + arm; false if busy/OOM
  void update();                       // read one I2S chunk per call (non-blocking)
  bool isRecording() const { return _recording; }
  bool isDone() const      { return _done; }

  // Valid only between isDone() and release().
  const uint8_t* getBuffer() const { return _buffer; }
  size_t getLength() const { return _used; }   // bytes of PCM captured

  void release();                      // free the recording buffer

private:
  uint8_t* _buffer = nullptr;
  size_t   _capacity = 0;
  size_t   _used = 0;
  uint32_t _deadlineMs = 0;
  bool _recording = false;
  bool _done = false;
  bool _installed = false;  // I2S driver up? guards every port access
};

extern AudioCapture audioCapture;
