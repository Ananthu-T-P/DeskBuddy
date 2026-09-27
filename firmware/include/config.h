#pragma once
// Local copy of config.h.example with PLACEHOLDER values so the project
// builds out of the box. This file is gitignored — put real values here,
// never commit them. main.cpp validates these at boot and shows the ERROR
// face with a clear Serial message if placeholders are still present.

#define WIFI_SSID "your-wifi-ssid"
#define WIFI_PASSWORD "your-wifi-password"

// Local IP of the machine running server/app.py, port 8000, /talk route.
#define SERVER_URL "http://192.168.1.50:8000/talk"

#define HTTP_TIMEOUT_MS 15000
#define MAX_RECORDING_MS 6000
