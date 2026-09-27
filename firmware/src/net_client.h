#pragma once
// net_client.h — POST recorded PCM to the relay server's /talk endpoint and
// collect the WAV reply (multipart/form-data, field "audio"; docs/SERVER.md).
//
// Blocking but bounded: every call has an explicit timeout (HTTP_TIMEOUT_MS,
// default 15000 — AGENTS.md §7). main.cpp draws the THINKING face immediately
// before and after the call (docs/FIRMWARE.md).

#include <Arduino.h>

enum class NetResult {
  OK,             // 200 with a non-empty audio body
  HTTP_ERROR,     // server answered with a non-200 status; see lastHttpCode()
  CONNECT_FAILED, // connect/request failed or timed out before an HTTP answer
  EMPTY_BODY,     // 200 but no body
  NO_MEMORY,      // couldn't allocate request/response buffers
  READ_TIMEOUT,   // connection stalled part-way through the response body
  TOO_LARGE,      // response bigger than the safety cap
};

class NetClient {
public:
  NetResult sendAudio(const uint8_t* pcm, size_t len, uint32_t timeoutMs);

  // Valid only after sendAudio() returns OK, until release().
  const uint8_t* responseData() const { return _resp; }
  size_t responseLength() const { return _respLen; }
  int lastHttpCode() const { return _lastCode; }

  void release();  // free the response buffer

private:
  uint8_t* _resp = nullptr;
  size_t _respLen = 0;
  int _lastCode = 0;
};

extern NetClient netClient;
