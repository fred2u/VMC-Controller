# Roadmap

The VMC Controller works every day. Since v1.0, every software item of this roadmap has been done (see [What is done](#what-is-done) and the [changelog](CHANGELOG.md)). What remains is mostly **hardware**, for a possible version 2 that is easier to reproduce.

| Priority | Meaning |
|---|---|
| 🔴 High | Missing piece that gets in the way during installation or when something breaks |
| 🟠 Medium | Clear improvement in comfort or reliability |
| 🟢 Low | Finishing touch |

---

## Still to do

### 🔴 Factory reset button on the enclosure

**Today**: **Forget Wi‑Fi** on the Maintenance page sends the ESP back to the `VMC-Setup` portal, but only while the page can be reached. If the ESP can no longer be reached at all (new Internet box and the portal timed out, unknown address…), the enclosure has to be opened and the ESP erased and reflashed over USB. No button can be reached with the enclosure closed.

**To do**:

- Add a button that can be reached from outside, recessed or pressed with a paperclip to avoid accidental presses. It can be wired to a free pin (GPIO13 for example) or to the board's `FLASH` button (GPIO0).
- Suggested behaviour:
  - short press: restart;
  - long press (about 10 seconds): forget the saved Wi‑Fi network and restart in setup mode, as **Forget Wi‑Fi** does;
  - possibly, reset the remote.
- Show each step with the LED (fast blinking during the long press, for example).
- Make room for the button in the 3D-printed enclosure.

### 🟠 More compact version

**Today**: the build is a prototype, with an ESP‑Mx DevKit development board, an FFC breakout, a ribbon cable and a home-made 50 × 50 mm PCB that did not fit perfectly. It all fits in an enclosure of about 62 × 62 × 42 mm.

**Ideas for a version 2**:

- **A single PCB** that directly carries:
  - a bare ESP module (ESP‑12F or ESP8285) instead of the development board;
  - an 8-pin 1.0 mm pitch FPC connector soldered on the board, to plug in the remote's ribbon without a breakout;
  - a USB socket (preferably USB‑C) and a 3.3 V regulator;
  - the reset button and a status LED.
- **Stacking**: put the remote board on top of the new PCB, or the other way round, rather than side by side.
- **Footprints to check** before ordering: measure the real dimensions of the parts (row spacing, connector positions) and print the PCB at 1:1 scale on paper to check the fit.
- **Programming**: add a serial header (RX, TX, GND, GPIO0, RST) for the first flash; the next updates go over Wi‑Fi.
- **Enclosure**: redesign it around the new PCB and publish the source file (STEP or Fusion 360) along with the STLs, so it can be modified.
- **Flash**: prefer a module with 4 MB of flash. With the 1 MB of the ESP8285, an update over Wi‑Fi only has room for a firmware of about 470 KB (about 410 KB today).

### 🟠 Circuit diagram

Add a circuit diagram, for example exported from EasyEDA, in addition to the PCB images and Gerber files.

### 🟢 Boot pins

GPIO2 and GPIO15 are also used to select the ESP8266 boot mode. If the remote's circuit pulls one of these pins the wrong way at boot, the ESP does not start. On a new board, prefer pins with no boot role (GPIO4, 5, 12, 13, 14), or check this point.

### 🟢 Displayed state

The mode shown is the one from the last command sent by the ESP; it is not read from the ventilation unit. A command sent by another remote, or the return to auto mode after a power cut, are not seen. This is a limit of the build, since the remote board only sends. The interface says "Last mode sent" so that it is clear. Reading the real state would need a radio receiver for the Orcon protocol, which is out of scope for now.

### 🟢 Pairing time

The 5 s that **Pair** holds `1` and `Auto` is a first guess (the manual only says "until the LED blinks"). If pairing does not work, adjust `pair_delay` in `src/main.cpp`, and note the right value here.

---

## What is done

### v1.1

- ✅ Updates over Wi‑Fi (ArduinoOTA), protected by a password from `VMC_OTA_PASSWORD`, with `_ota` environments in `platformio.ini`.
- ✅ Wi‑Fi setup from the `VMC-Setup` portal (ESPAsyncWiFiManager): `include/wifi.h` is gone, no network or password is stored in the project.
- ✅ Libraries moved to the maintained ESP32Async forks, pinned with the `espressif8266` platform.
- ✅ NodeMCU v3 test board with its own network name (`vmc-test`).
- ✅ Firmware version from the Git tags, answered on `/version` and `/info`, shown in the interface.
- ✅ Interface redesigned for smartphones without a CSS framework: light and dark themes, active mode highlighted, Online/Offline badge, board details.

### v1.2

- ✅ **Command reliability**: no `delay()` in `loop()` (state machine on `millis()`), a command received while another one is sent waits, and only the last one counts. The Max timer uses `millis()` and survives its wraparound.
- ✅ **Max feedback**: the commands answer the mode they set; Max in High answers `409` and is greyed out in the interface.
- ✅ **Remote pairing and reset** from the new Maintenance page, which also shows the ESP details and has **Restart** and **Forget Wi‑Fi**.
- ✅ **Security**: optional password for the web interface and every route (`VMC_HTTP_PASSWORD`), which also turns on a web update page, and for the `VMC-Setup` access point (`VMC_SETUP_PASSWORD`).
- ✅ **Native Home Assistant integration**: optional MQTT with discovery (`VMC_MQTT_HOST`), one button per command and a mode sensor.

### Before v1.0

- ✅ README with photos, wiring, bill of materials, enclosure and PCB files, troubleshooting.
- ✅ Unit tests on the computer and GitHub Actions CI (tests, firmware builds, LittleFS image).
- ✅ Web interface in English, small fixes (`<meta charset>`, CSS typo, mode shown at once).
