// audio_playback.cpp — MAX98357A on I2S port 1 (pins from HARDWARE.md/pins.h).
// update() writes a small chunk per loop() iteration with a bounded wait, so
// the TALKING mouth animation keeps running smoothly (AGENTS.md §4/§7).

#include "audio_playback.h"
#include "pins.h"

#include <driver/i2s.h>
#include <esp_heap_caps.h>

AudioPlayback audioPlayback;

namespace {
// The mic uses I2S_NUM_0; the amp gets the second peripheral (HARDWARE.md).
constexpr i2s_port_t AMP_PORT  = I2S_NUM_1;
constexpr uint32_t SAMPLE_RATE = 16000;
constexpr size_t   MAX_CLIP_BYTES = 512 * 1024;  // hard cap, ~16 s of audio
constexpr size_t   WRITE_CHUNK    = 2048;        // bytes per i2s_write() call
constexpr uint32_t DRAIN_MARGIN_MS = 300;        // DMA buffer drain allowance
constexpr size_t   WAV_HEADER_BYTES = 44;        // standard RIFF/WAVE header
}  // namespace

bool AudioPlayback::begin() {
  i2s_config_t cfg = {};
  cfg.mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate          = SAMPLE_RATE;
  cfg.bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT;  // mono
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count        = 8;
  cfg.dma_buf_len          = 512;
  cfg.use_apll             = false;
  cfg.tx_desc_auto_clear   = true;
  cfg.fixed_mclk           = 0;
  if (i2s_driver_install(AMP_PORT, &cfg, 0, nullptr) != ESP_OK) return false;

  i2s_pin_config_t pinCfg = {};
  pinCfg.mck_io_num    = I2S_PIN_NO_CHANGE;
  pinCfg.bck_io_num    = PIN_AMP_BCLK;
  pinCfg.ws_io_num     = PIN_AMP_LRC;
  pinCfg.data_out_num  = PIN_AMP_DIN;
  pinCfg.data_in_num   = I2S_PIN_NO_CHANGE;
  if (i2s_set_pin(AMP_PORT, &pinCfg) != ESP_OK) {
    i2s_driver_uninstall(AMP_PORT);
    return false;
  }
  i2s_zero_dma_buffer(AMP_PORT);
  i2s_start(AMP_PORT);
  _installed = true;
  return true;
}

bool AudioPlayback::play(const uint8_t* wavBytes, size_t len) {
  stop();
  if (!_installed || !wavBytes || len == 0) return false;

  // Skip the 44-byte WAV header if this is a RIFF file; otherwise treat the
  // whole buffer as raw PCM (both are 16 kHz / 16-bit / mono by contract).
  const uint8_t* pcm = wavBytes;
  size_t pcmLen = len;
  if (len >= WAV_HEADER_BYTES && memcmp(wavBytes, "RIFF", 4) == 0) {
    pcm    += WAV_HEADER_BYTES;
    pcmLen -= WAV_HEADER_BYTES;
  }
  if (pcmLen == 0 || pcmLen > MAX_CLIP_BYTES) return false;

  // Copy so the caller's buffer can be freed immediately.
  _pcm = (uint8_t*)heap_caps_malloc(pcmLen, MALLOC_CAP_SPIRAM);
  if (!_pcm) _pcm = (uint8_t*)malloc(pcmLen);  // fallback: internal RAM
  if (!_pcm) return false;
  memcpy(_pcm, pcm, pcmLen);

  _len        = pcmLen;
  _offset     = 0;
  _durationMs = (uint32_t)((uint64_t)pcmLen * 1000 / (SAMPLE_RATE * 2));
  _startMs    = millis();
  _playing    = true;
  i2s_zero_dma_buffer(AMP_PORT);
  return true;
}

void AudioPlayback::update() {
  if (!_installed || !_playing) return;

  if (_offset < _len) {
    size_t n = _len - _offset;
    if (n > WRITE_CHUNK) n = WRITE_CHUNK;
    size_t written = 0;
    // Bounded wait (<= ~20 ms) — short enough that the face stays smooth.
    i2s_write(AMP_PORT, _pcm + _offset, n, &written, pdMS_TO_TICKS(20));
    _offset += written;
  } else if (millis() - _startMs >= _durationMs + DRAIN_MARGIN_MS) {
    // Every sample handed to DMA and the DMA buffers have drained: done.
    stop();
  }
}

void AudioPlayback::stop() {
  if (_pcm) {
    free(_pcm);
    _pcm = nullptr;
  }
  _len = 0;
  _offset = 0;
  _durationMs = 0;
  _playing = false;
  if (_installed) i2s_zero_dma_buffer(AMP_PORT);  // silence DMA ring residue
}
