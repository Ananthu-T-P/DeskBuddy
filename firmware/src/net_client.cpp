// net_client.cpp — one blocking-but-bounded HTTP round trip to the server.
// Every failure path returns a distinct NetResult; nothing here can hang the
// device longer than timeoutMs (AGENTS.md §7).

#include "net_client.h"
#include "config.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <esp_heap_caps.h>

NetClient netClient;

namespace {
// Safety cap on the WAV reply (~16 s of 16 kHz / 16-bit audio; normal replies
// to 1–2 short Malayalam sentences are far smaller).
constexpr size_t MAX_RESPONSE_BYTES = 512 * 1024;
const char BOUNDARY[] = "----ESP32DesktopPetBoundary7f3a";
}  // namespace

void NetClient::release() {
  if (_resp) {
    free(_resp);
    _resp = nullptr;
  }
  _respLen = 0;
}

NetResult NetClient::sendAudio(const uint8_t* pcm, size_t len, uint32_t timeoutMs) {
  release();
  _lastCode = 0;
  if (!pcm || len == 0) return NetResult::EMPTY_BODY;

  // ---- Build the multipart/form-data body in one buffer ----------------
  const String head = String("--") + BOUNDARY +
      "\r\nContent-Disposition: form-data; name=\"audio\"; filename=\"audio.raw\"\r\n"
      "Content-Type: application/octet-stream\r\n\r\n";
  const String tail = String("\r\n--") + BOUNDARY + "--\r\n";

  const size_t bodyLen = head.length() + len + tail.length();
  uint8_t* body = (uint8_t*)heap_caps_malloc(bodyLen, MALLOC_CAP_SPIRAM);
  if (!body) body = (uint8_t*)malloc(bodyLen);  // fallback: internal RAM
  if (!body) return NetResult::NO_MEMORY;
  memcpy(body, head.c_str(), head.length());
  memcpy(body + head.length(), pcm, len);
  memcpy(body + head.length() + len, tail.c_str(), tail.length());

  // ---- POST it, bounded by the configured timeout ----------------------
  HTTPClient http;
  http.setTimeout(timeoutMs);  // TCP connect + request timeout, ms
  if (!http.begin(SERVER_URL)) {
    free(body);
    return NetResult::CONNECT_FAILED;
  }
  http.addHeader("Content-Type", String("multipart/form-data; boundary=") + BOUNDARY);

  const int code = http.POST(body, bodyLen);
  _lastCode = code;
  free(body);
  if (code <= 0)            { http.end(); return NetResult::CONNECT_FAILED; }
  if (code != HTTP_CODE_OK) { http.end(); return NetResult::HTTP_ERROR; }

  // ---- Stream the WAV reply into a capped buffer -----------------------
  const int expected = http.getSize();  // Content-Length; -1 if chunked
  if (expected == 0)                          { http.end(); return NetResult::EMPTY_BODY; }
  if (expected > (int)MAX_RESPONSE_BYTES)     { http.end(); return NetResult::TOO_LARGE; }
  const size_t cap = (expected > 0) ? (size_t)expected : MAX_RESPONSE_BYTES;

  _resp = (uint8_t*)heap_caps_malloc(cap, MALLOC_CAP_SPIRAM);
  if (!_resp) _resp = (uint8_t*)malloc(cap);
  if (!_resp) { http.end(); return NetResult::NO_MEMORY; }

  WiFiClient* stream = http.getStreamPtr();
  const uint32_t deadline = millis() + timeoutMs;
  _respLen = 0;
  for (;;) {
    const int avail = stream->available();
    if (avail > 0) {
      const size_t room = cap - _respLen;
      if (room == 0) break;
      const int want = (avail < (int)room) ? avail : (int)room;
      const int got = stream->read(_resp + _respLen, (size_t)want);
      if (got <= 0) break;
      _respLen += (size_t)got;
      continue;
    }
    if (expected > 0 && _respLen >= (size_t)expected) break;
    if (!http.connected()) break;
    if ((int32_t)(millis() - deadline) >= 0) break;
    delay(1);  // <= a few ms; LAN round trips are fast
  }
  http.end();

  if (_respLen == 0)                              { release(); return NetResult::EMPTY_BODY; }
  if (expected > 0 && _respLen < (size_t)expected) { release(); return NetResult::READ_TIMEOUT; }
  return NetResult::OK;
}
