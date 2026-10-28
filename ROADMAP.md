# Roadmap

The VMC Controller works every day, but it is still a **prototype**. This document lists what is missing to turn it into a more polished version that is easy to reproduce and maintain. It is the starting point for a possible version 2, as the current version is not meant to change.

Items are grouped by topic. The priority shows how much it matters to someone reproducing the project.

| Priority | Meaning |
|---|---|
| 🔴 High | Missing piece that gets in the way during installation or when something breaks |
| 🟠 Medium | Clear improvement in comfort or reliability |
| 🟢 Low | Finishing touch |
| ✅ | Done |

---

## 1. Remote features

### 🔴 Remote pairing mode

**Today**: to pair the remote with the ventilation unit (after replacing the unit or after a reset, for example), `1` and `Auto` must be pressed **at the same time**. The firmware only sends one press at a time, so the enclosure has to be opened and the contacts bridged by hand.

**To do**:

- Add a `/pair` route (and a button in the interface, possibly tucked away in a "Maintenance" page) that pulls GPIO2 (`1`) and GPIO5 (`Auto`) LOW at the same time, until the remote's LED blinks. The exact duration has to be found by trial: the manual only says "until the LED blinks".
- Show the procedure on the ventilation unit side in the interface: unplug the unit for 5 seconds, plug it back in, then start pairing within 3 minutes.
- In the same way, add the **remote reset** (`Default` and `Timer` pressed together for 3 seconds).
- Make `sendCommand` able to press several buttons at once and to hold the press longer, which both features need.

### 🟠 Feedback on the Max modes

- Max commands are silently ignored when the current mode is High. This should at least be reported in the HTTP response and in the interface. The behaviour may be on purpose (the unit is already in the high position), but it is not written down anywhere.
- The command routes answer `200` before running the command. A response that gives the resulting mode, or an error, would be more useful to Home Assistant.

---

## 2. Hardware

### 🔴 Factory reset button on the enclosure

**Today**: no button can be reached with the enclosure closed. After changing the Internet box or the Wi‑Fi password, the enclosure has to be opened and the ESP reflashed.

**To do**:

- Add a button that can be reached from outside, recessed or pressed with a paperclip to avoid accidental presses. It can be wired to a free pin (GPIO13 for example) or to the board's `FLASH` button (GPIO0).
- Suggested behaviour:
  - short press: restart;
  - long press (about 10 seconds): erase the saved Wi‑Fi settings and go back to setup mode (see the Wi‑Fi section below);
  - possibly, reset the remote (see above).
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
- **Programming**: add a serial header (RX, TX, GND, GPIO0, RST) for the first flash, then switch to OTA.
- **Enclosure**: redesign it around the new PCB and publish the source file (STEP or Fusion 360) along with the STLs, so it can be modified.

### 🟢 Boot pins

GPIO2 and GPIO15 are also used to select the ESP8266 boot mode. If the remote's circuit pulls one of these pins the wrong way at boot, the ESP does not start. On a new board, prefer pins with no boot role (GPIO4, 5, 12, 13, 14), or check this point.

---

## 3. Software

### ✅ Wi‑Fi setup with a library

~~The network name and password were hard-coded in `include/wifi.h`.~~ [ESPAsyncWiFiManager](https://github.com/alanswx/ESPAsyncWiFiManager) is in place: when the ESP cannot reach a saved network, it opens the `VMC-Setup` access point with a captive portal for 3 minutes, see "Connect to Wi‑Fi" in the README. `include/wifi.h` is now optional (OTA password and fallback network).

Still to do:

- Erase the saved network with the factory reset button (see "Factory reset" above) or from a maintenance page.
- Optionally protect the `VMC-Setup` access point with a password.

### ✅ Updates over Wi‑Fi (OTA)

~~Add ArduinoOTA or a web update page (`/update`), to update the firmware and the interface without opening the enclosure or plugging in a cable.~~ ArduinoOTA is in place, protected by `OTA_PASSWORD`, see the README. A web update page (`/update`) could still be added, to update without PlatformIO.

### 🟠 Native Home Assistant integration

Today, the integration goes through a manual `rest_command` configuration. An MQTT integration with Home Assistant auto-discovery would directly create a "VMC" device with its buttons and a mode sensor. It would also give the state without polling `/mode` over and over.

### 🟠 Command reliability

- **Lost commands**: only one command is stored and it is cleared once it has run. A command received while another one is being sent (up to about 3 seconds for Max 60 min) is lost. A small queue is needed, or at least rejecting the request with a `409` or `503` code.
- **Shared variables**: the current command is a `String` written by the async server and read by `loop()` without protection. A plain variable (an integer or an enum) marked `volatile` would be safer.
- **Blocking delays**: `loop()` uses `delay()` while sending the pulses. A state machine based on `millis()` would keep the ESP responsive.
- **Max mode duration**: the end of the Max mode is computed with `time(0)`, without NTP sync. Using `millis()` would be simpler and safer.

### 🟢 Displayed state

The mode shown is the one from the last command sent by the ESP, and it is not read from the ventilation unit. A command sent by another remote, or the return to auto mode after a power cut, are not seen. This is a limit of the build, since the remote receives nothing. It should at least be stated in the interface ("last mode sent").

✅ The interface now shows "Last mode sent" instead of "Current mode". The limitation itself remains.

### 🟢 Security

There is no authentication. That is acceptable on a trusted local network, but there should at least be optional HTTP basic authentication, especially if OTA is added, since OTA would allow the firmware to be replaced.

---

## 4. Web interface

- ✅ ~~Use one language: the buttons are in English ("Low", "Medium", "High"), except "Automatique", and the page is declared as French.~~ Everything is in English: "Automatique" becomes "Auto" (also in the `/mode` response) and the page is declared as English.
- ✅ ~~Highlight the active mode on its button, in addition to the "Current mode" text.~~ The active mode's button is filled.
- ✅ ~~Fix a typo in `data/style.css`: `margin: 0 10px O O;` uses the letter `O` instead of the digit `0`, which makes the rule invalid.~~ Replaced with `margin: 0 10px;`.
- ✅ Show the mode as soon as the page opens, without waiting for the first 2-second refresh.
- ✅ Fix the `<meta>` tag in `index.html`, where `charset` was stuck to the `viewport` attribute and therefore ignored.
- 🟢 Add a "Maintenance" page: pairing, reset, system information (version, IP address, Wi‑Fi signal strength, uptime).

---

## 5. Project and documentation

- 🟠 Add a circuit diagram, for example exported from EasyEDA, in addition to the PCB.
- ✅ ~~Add an automatic build check (GitHub Actions with PlatformIO), to make sure the firmware still builds with recent library versions.~~ Done along with the unit tests (`.github/workflows/ci.yml`).
- ✅ ~~Pin the ESP Async WebServer library version in `platformio.ini`. Without a version, a library update can break the build.~~ Done: the libraries now come from the maintained ESP32Async forks, pinned to a release, and the `espressif8266` platform is pinned too.
- 🟢 Number the firmware versions (Git tags) and show the version in the interface.
