#pragma once
// PLACEHOLDER values — edit these on the machine that flashes/runs the
// firmware, and never push the edited file back (AGENTS.md §8).
// Mahoraga.ino validates them at boot and shows the ERROR face + a clear
// Serial message if placeholders are still present.

#define WIFI_SSID "your-wifi-ssid"
#define WIFI_PASSWORD "your-wifi-password"

// Where the relay server is reachable (/talk route). Default: a machine on
// the same WiFi network — replace with that machine's local IP
// (ipconfig / ifconfig), e.g. "http://192.168.1.50:8000/talk".
// https:// URLs (e.g. a public tunnel) also work via WiFiClientSecure.
#define SERVER_URL "http://192.168.1.50:8000/talk"

#define HTTP_TIMEOUT_MS 15000
#define MAX_RECORDING_MS 6000
