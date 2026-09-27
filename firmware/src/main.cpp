// main.cpp — top-level orchestration ONLY (docs/FIRMWARE.md).
//
// State machine: IDLE -> LISTENING -> THINKING -> TALKING -> IDLE, with
// ERROR reachable from any state. All I2S, HTTP and pixel work lives in the
// modules; this file only calls their APIs and petFace.setState().
//
// Non-blocking loop (AGENTS.md §7): every long operation (recording,
// playback) is chunked through update() calls; the only blocking call is the
// bounded HTTP POST in THINKING (docs/FIRMWARE.md explicitly allows this).

#include <Arduino.h>

#include "config.h"
#include "pins.h"
#include "pet_face.h"
#include "wifi_manager.h"
#include "audio_capture.h"
#include "audio_playback.h"
#include "net_client.h"

// ---------------------------------------------------------------------------
// Standalone OLED test mode (docs/TESTING.md §1).
// Set to 1 and flash with ONLY the OLED wired: the face cycles through all
// five states, 3 s each, with no WiFi/audio/network involved.
// Set back to 0 for normal operation.
#define FACE_SELFTEST 0

namespace {

#if FACE_SELFTEST
const PetState SELFTEST_ORDER[5] = {
  PetState::IDLE, PetState::LISTENING, PetState::THINKING,
  PetState::TALKING, PetState::ERROR,
};
#endif

constexpr uint32_t ERROR_FACE_MS = 3000;  // transient errors: X_X face, then IDLE
constexpr uint32_t DEBOUNCE_MS   = 50;

PetState state          = PetState::IDLE;
uint32_t errorShownMs   = 0;
bool     errorPersistent = false;  // persistent = hold ERROR until WiFi is back

void setState(PetState s, const char* why) {
  state = s;
  petFace.setState(s);
  Serial.printf("[state] -> %d (%s)\n", (int)s, why);
}

void goError(bool persistent, const char* why) {
  errorPersistent = persistent;
  errorShownMs = millis();
  setState(PetState::ERROR, why);
}

// Debounced, edge-detected read of the active-low BOOT button.
bool buttonPressedEdge() {
  static int rawLast = HIGH;
  static int stable  = HIGH;
  static uint32_t lastChangeMs = 0;

  bool edge = false;
  const int raw = digitalRead(PIN_BUTTON);
  if (raw != rawLast) {
    rawLast = raw;
    lastChangeMs = millis();
  }
  if (raw != stable && millis() - lastChangeMs >= DEBOUNCE_MS) {
    stable = raw;
    if (stable == LOW) edge = true;  // BOOT button: pressed = LOW
  }
  return edge;
}

// Boot-time placeholder detection (AGENTS.md §7 — config validation).
bool configLooksValid() {
  if (strlen(WIFI_SSID) == 0 || strcmp(WIFI_SSID, "your-wifi-ssid") == 0) return false;
  if (strcmp(WIFI_PASSWORD, "your-wifi-password") == 0) return false;
  const String url = SERVER_URL;
  if (!url.startsWith("http://") && !url.startsWith("https://")) return false;
  if (url.indexOf("192.168.1.50") >= 0) return false;  // example placeholder IP
  return true;
}

const char* netResultName(NetResult r) {
  switch (r) {
    case NetResult::OK:             return "OK";
    case NetResult::HTTP_ERROR:     return "HTTP_ERROR";
    case NetResult::CONNECT_FAILED: return "CONNECT_FAILED";
    case NetResult::EMPTY_BODY:     return "EMPTY_BODY";
    case NetResult::NO_MEMORY:      return "NO_MEMORY";
    case NetResult::READ_TIMEOUT:   return "READ_TIMEOUT";
    case NetResult::TOO_LARGE:      return "TOO_LARGE";
  }
  return "?";
}

}  // namespace

// --------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(300);  // setup-only: lets USB serial attach. loop() is delay()-free.
  Serial.println("\n[boot] desktop pet firmware starting");

  pinMode(PIN_BUTTON, INPUT_PULLUP);

  if (!petFace.begin())
    Serial.println("[boot] OLED init FAILED — check wiring, or try address 0x3D (pins.h)");

#if FACE_SELFTEST
  Serial.println("[boot] FACE_SELFTEST=1 — cycling all 5 face states, 3 s each (OLED only)");
  return;  // skip WiFi/audio init entirely — docs/TESTING.md §1
#endif

  if (!configLooksValid()) {
    Serial.println("[boot] CONFIG ERROR: include/config.h still has placeholder values.");
    Serial.println("       Edit include/config.h with real WiFi + SERVER_URL, then reflash.");
    // Persistent ERROR: WiFi can never connect with placeholder credentials,
    // so the face keeps showing X_X until reflashed with a real config.
    goError(true, "invalid config.h");
  }

  wifiManager.begin(WIFI_SSID, WIFI_PASSWORD);
  if (!audioCapture.begin())  Serial.println("[boot] mic I2S init FAILED (check INMP441 wiring)");
  if (!audioPlayback.begin()) Serial.println("[boot] amp I2S init FAILED (check MAX98357A wiring)");
}

// --------------------------------------------------------------------------
void loop() {
#if FACE_SELFTEST
  // Cycle every state on a timer; nothing else runs.
  static uint32_t lastSwitchMs = 0;
  static int idx = -1;
  if (millis() - lastSwitchMs >= 3000) {
    lastSwitchMs = millis();
    idx = (idx + 1) % 5;
    setState(SELFTEST_ORDER[idx], "selftest");
  }
  petFace.update();
  return;
#endif

  wifiManager.update();
  petFace.update();

  const bool pressed = buttonPressedEdge();
  const bool wifiUp  = wifiManager.isConnected();

  switch (state) {

    case PetState::IDLE:
      if (!wifiUp) {
        goError(true, "wifi down");
        break;
      }
      if (pressed) {
        if (audioCapture.startRecording(MAX_RECORDING_MS)) {
          setState(PetState::LISTENING, "button pressed");
        } else {
          goError(false, "recording buffer alloc failed");
        }
      }
      break;

    case PetState::LISTENING:
      audioCapture.update();
      if (!wifiUp) {
        audioCapture.release();
        goError(true, "wifi lost mid-recording");
        break;
      }
      if (audioCapture.isDone()) {
        // Mic sanity dump for TESTING.md §3: peak amplitude should jump when
        // you speak near the INMP441.
        const int16_t* samples = (const int16_t*)audioCapture.getBuffer();
        const size_t n = audioCapture.getLength() / 2;
        int mn = 32767, mx = -32768;
        for (size_t i = 0; i < n; i++) {
          if (samples[i] < mn) mn = samples[i];
          if (samples[i] > mx) mx = samples[i];
        }
        Serial.printf("[mic] captured %u bytes, min=%d max=%d\n",
                      (unsigned)audioCapture.getLength(), mn, mx);

        if (n == 0) {
          audioCapture.release();
          goError(false, "no audio captured");
          break;
        }
        setState(PetState::THINKING, "recording complete");
      }
      break;

    case PetState::THINKING: {
      // Blocking-but-bounded HTTP call (docs/FIRMWARE.md): draw one THINKING
      // frame going in, one coming out; the call itself cannot interleave.
      petFace.update();
      NetResult r = netClient.sendAudio(audioCapture.getBuffer(),
                                        audioCapture.getLength(),
                                        HTTP_TIMEOUT_MS);
      audioCapture.release();
      petFace.update();

      if (r == NetResult::OK) {
        const bool playing = audioPlayback.play(netClient.responseData(),
                                                netClient.responseLength());
        netClient.release();  // play() made its own copy
        if (playing) setState(PetState::TALKING, "server replied");
        else         goError(false, "playback start failed");
      } else {
        Serial.printf("[net] /talk failed: %s (http=%d)\n",
                      netResultName(r), netClient.lastHttpCode());
        netClient.release();
        goError(false, "server request failed");
      }
      break;
    }

    case PetState::TALKING:
      audioPlayback.update();
      if (!audioPlayback.isPlaying()) setState(PetState::IDLE, "reply finished");
      break;

    case PetState::ERROR:
      if (errorPersistent) {
        if (wifiUp) setState(PetState::IDLE, "wifi recovered");
      } else if (millis() - errorShownMs >= ERROR_FACE_MS) {
        setState(PetState::IDLE, "error shown, recovering");
      }
      break;
  }
}
