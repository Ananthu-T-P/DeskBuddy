#pragma once
// wifi_manager.h — non-blocking WiFi connect / monitor / reconnect
// with exponential backoff (AGENTS.md §7). Call update() every loop().

#include <Arduino.h>
#include <WiFi.h>

class WifiManager {
public:
  void begin(const char* ssid, const char* password);
  void update();                    // rate-limited internally; cheap to call
  bool isConnected() const { return _connected; }
  IPAddress localIP() const { return WiFi.localIP(); }

private:
  const char* _ssid = "";
  const char* _pass = "";
  bool     _connected = false;
  uint32_t _lastCheckMs   = 0;
  uint32_t _lastAttemptMs = 0;
  uint32_t _backoffMs     = 2000;

  static constexpr uint32_t CHECK_INTERVAL_MS = 500;
  static constexpr uint32_t MAX_BACKOFF_MS    = 30000;
};

extern WifiManager wifiManager;
