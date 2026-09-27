# DeskBuddy

ESP32-S3 desk buddy that talks Malayalam. Press the button, talk to it, it
talks back through a small speaker, and the little OLED face animates the
whole time (idle / listening / thinking / talking / error) so you can tell
what it's doing.

Everything runs on the free Gemini API - it hears the audio, thinks of a
reply, and speaks it. One API key is all you need. The "server" is just a
small Python program on any laptop on the same wifi; it shuttles audio
between the ESP32 and Gemini.

## what's in here

- `firmware/Mahoraga/` - the arduino sketch (open `Mahoraga.ino`)
- `server/` - the python relay (`app.py` is the entry point)
- `HARDWARE.md` - wiring/pinout, build it exactly like this
- `docs/` - more detail if you want it (`TESTING.md` is the hardware-day
  checklist)

## quick start

### 1. wire it

Follow `HARDWARE.md`. Note the OLED goes on GPIO41/42, not the usual
21/22 - check the table.

### 2. gemini key

Get a free key at aistudio.google.com. That's the only key this project
needs.

### 3. server (any machine with python 3.10+)

```
cd server
python -m venv venv
venv\Scripts\activate       :: mac/linux: source venv/bin/activate
pip install -r requirements.txt
notepad .env                :: paste the gemini key
python -m uvicorn app:app --host 0.0.0.0 --port 8000
```

First run, windows firewall asks to allow python - allow it on private
networks. Then from your phone on the same wifi open
`http://<laptop-ip>:8000/health` and you should get `{"status":"ok"}`.
(`ipconfig` on the laptop shows its ip)

### 4. firmware (arduino ide)

- boards manager: install "esp32 by Espressif"; library manager: install
  "Adafruit SSD1306" and "Adafruit GFX Library"
- open `firmware/Mahoraga/Mahoraga.ino` (the other files open as tabs)
- fill in the `config.h` tab: wifi name/password and
  `SERVER_URL = http://<laptop-ip>:8000/talk`
- board = "ESP32S3 Dev Module", and set "USB CDC On Boot" = Enabled
  (otherwise serial monitor shows nothing)
- upload

### 5. use it

Press the BOOT button and talk - malayalam works best. The face goes
listening -> thinking -> talking and back to idle. Serial monitor (115200)
logs what the board is doing, and the server window prints what it heard
and what it replied.

## secrets

`firmware/Mahoraga/config.h` and `server/.env` are in the repo with
PLACEHOLDER values on purpose, so the whole thing is visible. Put the real
wifi/gemini values in only on the machine that runs it, and don't push
those edits back.

## cost

Hardware is a one-time ~Rs.1000-1600 (list in `HARDWARE.md`). Gemini's free
tier covers casual use easily. If google asks you to "enable billing" to
unlock the free tier, it still charges Rs.0 inside free limits (set a zero
budget alert if you want to be sure).

## if it breaks

- ERROR face right after boot = it can't reach the server. Check SERVER_URL,
  check uvicorn is running, check /health from a phone on the same wifi.
- Powershell refuses to activate the venv: run once
  `Set-ExecutionPolicy -Scope CurrentUser RemoteSigned`
- OLED blank from the start? Try address 0x3D instead of 0x3C in pins.h.
- Serial or server log shows why something failed - that's what they're for.
- Proper test order on hardware day: `docs/TESTING.md` - face first, then
  wifi, then mic, then server, then the full loop. Set FACE_SELFTEST to 1 in
  the sketch to watch all the face animations with nothing else wired.
