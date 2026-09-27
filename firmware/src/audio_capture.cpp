// audio_capture.cpp — INMP441 mic on I2S port 0 (pins from HARDWARE.md/pins.h).
// Recording is chunked: update() reads a small block per loop() iteration so
// the OLED face and the rest of the loop keep running (AGENTS.md §7).

#include "audio_capture.h"
#include "pins.h"

#include <driver/i2s.h>
#include <esp_heap_caps.h>

AudioCapture audioCapture;

namespace {
// The amp uses I2S_NUM_1; the mic gets its own peripheral (HARDWARE.md).
constexpr i2s_port_t MIC_PORT  = I2S_NUM_0;
constexpr uint32_t SAMPLE_RATE = 16000;
constexpr int      CHUNK_WORDS = 128;  // int32_t words read per update()
}  // namespace

bool AudioCapture::begin() {
  i2s_config_t cfg = {};
  cfg.mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate          = SAMPLE_RATE;
  cfg.bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT;  // INMP441: 24-bit data in 32-bit slots
  cfg.channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT;  // L/R tied to GND = left channel
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count        = 8;
  cfg.dma_buf_len          = 256;
  cfg.use_apll             = false;
  cfg.tx_desc_auto_clear   = false;
  cfg.fixed_mclk           = 0;
  if (i2s_driver_install(MIC_PORT, &cfg, 0, nullptr) != ESP_OK) return false;

  i2s_pin_config_t pinCfg = {};
  pinCfg.mck_io_num    = I2S_PIN_NO_CHANGE;
  pinCfg.bck_io_num    = PIN_MIC_SCK;
  pinCfg.ws_io_num     = PIN_MIC_WS;
  pinCfg.data_out_num  = I2S_PIN_NO_CHANGE;
  pinCfg.data_in_num   = PIN_MIC_SD;
  if (i2s_set_pin(MIC_PORT, &pinCfg) != ESP_OK) {
    i2s_driver_uninstall(MIC_PORT);
    return false;
  }
  i2s_start(MIC_PORT);
  _installed = true;
  return true;
}

bool AudioCapture::startRecording(uint32_t maxMs) {
  if (!_installed || _recording) return false;
  release();

  // Hard cap on length regardless of button behaviour (AGENTS.md §7).
  const size_t bytes = (size_t)((uint64_t)SAMPLE_RATE * 2 * maxMs / 1000);
  _buffer = (uint8_t*)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
  if (!_buffer) _buffer = (uint8_t*)malloc(bytes);  // fallback: internal RAM
  if (!_buffer) return false;

  _capacity   = bytes;
  _used       = 0;
  _deadlineMs = millis() + maxMs;
  _done       = false;

  // Flush stale samples sitting in the DMA buffers.
  int32_t discard[CHUNK_WORDS];
  size_t br = 0;
  for (int i = 0; i < 8; i++) i2s_read(MIC_PORT, discard, sizeof(discard), &br, 0);

  _recording = true;
  return true;
}

void AudioCapture::update() {
  if (!_installed || !_recording || _done) return;

  if ((int32_t)(millis() - _deadlineMs) >= 0 || _used >= _capacity) {
    _recording = false;
    _done = true;
    return;
  }

  int32_t raw[CHUNK_WORDS];
  size_t bytesRead = 0;
  // Short bounded wait (<= ~5 ms) keeps loop() non-blocking (AGENTS.md §7).
  i2s_read(MIC_PORT, raw, sizeof(raw), &bytesRead, pdMS_TO_TICKS(5));

  const size_t words = bytesRead / sizeof(int32_t);
  for (size_t i = 0; i < words && _used + 2 <= _capacity; i++) {
    // 24-bit-in-32 -> 16-bit with a small gain boost, clamped.
    int32_t v = raw[i] >> 14;
    if (v > 32767)  v = 32767;
    if (v < -32768) v = -32768;
    const int16_t s = (int16_t)v;
    _buffer[_used++] = (uint8_t)(s & 0xFF);          // little-endian PCM
    _buffer[_used++] = (uint8_t)((s >> 8) & 0xFF);
  }
}

void AudioCapture::release() {
  if (_buffer) {
    free(_buffer);  // free() is correct for heap_caps_malloc'd memory too
    _buffer = nullptr;
  }
  _capacity = 0;
  _used = 0;
  _recording = false;
  _done = false;
}
