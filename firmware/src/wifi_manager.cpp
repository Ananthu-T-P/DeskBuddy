// wifi_manager.cpp — connect once in begin(), then update() watches the
// link and retries with exponential backoff instead of hanging (AGENTS.md §7).

#include "wifi_manager.h"

WifiManager wifiManager;

void WifiManager::begin(const char* ssid, const char* password) {
  _ssid = ssid;
  _pass = password;
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);          // lower latency for voice round-trips
  WiFi.disconnect(true);         // clear any stale state from a previous boot
  WiFi.begin(_ssid, _pass);
  _lastAttemptMs = millis();
  Serial.printf("[WiFi] connecting to '%s'...\n", _ssid);
}

void WifiManager::update() {
  const uint32_t now = millis();
  if (now - _lastCheckMs < CHECK_INTERVAL_MS) return;
  _lastCheckMs = now;

  const wl_status_t st = WiFi.status();
  if (st == WL_CONNECTED) {
    if (!_connected) {
      _connected = true;
      _backoffMs = 2000;  // reset backoff after a successful connect
      Serial.printf("[WiFi] connected, IP: %s\n",
                    WiFi.localIP().toString().c_str());
    }
    return;
  }

  if (_connected) {
    _connected = false;
    Serial.println("[WiFi] connection lost");
  }
  if (now - _lastAttemptMs >= _backoffMs) {
    Serial.printf("[WiFi] retrying (status=%d, backoff=%ums)...\n",
                  (int)st, (unsigned)_backoffMs);
    WiFi.disconnect(false);
    WiFi.begin(_ssid, _pass);
    _lastAttemptMs = now;
    _backoffMs = min(_backoffMs * 2, MAX_BACKOFF_MS);
  }
}
