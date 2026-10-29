# Changelog

What changed in each version of the VMC Controller. Versions are Git tags (see "Releases" in the [README](README.md#releases)). The firmware and the web interface go together: after an update, upload both (`upload` and `uploadfs`, or both files on the Maintenance page).

## v1.2

Needs nothing special coming from v1.1: update the firmware and the web interface over Wi‑Fi.

### Added

- Maintenance page (`/maintenance`): **Pair** the remote (`POST /pair`, `1` and `Auto` held 5 s), **Reset the remote** (`POST /reset-remote`, `Default` and `Timer` held 4 s), ESP details, **Restart** (`POST /restart`) and **Forget Wi‑Fi** (`POST /forget-wifi`). Each action asks for confirmation.
- Optional password for the web interface and every HTTP route, user `admin` (`VMC_HTTP_PASSWORD`).
- Web update of the firmware or the web interface from the Maintenance page (`/update/firmware`, `/update/web`), only when built with `VMC_HTTP_PASSWORD`.
- Optional password for the `VMC-Setup` access point (`VMC_SETUP_PASSWORD`).
- Optional MQTT with Home Assistant discovery (`VMC_MQTT_HOST`, `VMC_MQTT_PORT`, `VMC_MQTT_USER`, `VMC_MQTT_PASSWORD`): topics `<name>/command`, `<name>/mode`, `<name>/status`.
- `/info` also answers the Wi‑Fi network, uptime, free memory, reason of the last start and whether the web update is built in.

### Changed

- Commands are sent without blocking: the web server and OTA keep answering while a button is pressed. A command received meanwhile waits, and only the last one is sent.
- Command routes answer the mode they set as `text/plain` (for example `Max 30 min`). Max in High answers `409` and presses nothing.
- `scripts/ota_password.py` is now `scripts/passwords.py`.

## v1.1

**Needs one USB flash** coming from v1.0, firmware and web interface (`upload` then `uploadfs`): the LittleFS area of the production board moved to make room for updates over Wi‑Fi.

### Added

- Updates over Wi‑Fi (ArduinoOTA), protected by `VMC_OTA_PASSWORD`, and `espmxdevkit_ota` / `nodemcuv2_ota` environments.
- Wi‑Fi setup portal `VMC-Setup` (ESPAsyncWiFiManager) instead of `include/wifi.h`.
- NodeMCU v3 test environment (`nodemcuv2`) with its own network name, `vmc-test`.
- Firmware version from the Git tags, on the serial port, `/version`, `/info` and in the interface.
- `/info` route (JSON): version, network name, IP address, Wi‑Fi signal.

### Changed

- Interface redesigned for smartphones, without Milligram: light and dark themes, buttons grouped by Speed / Program / Max, Online/Offline badge, board details at the bottom.
- Libraries: maintained ESP32Async forks of ESPAsyncTCP and ESPAsyncWebServer, pinned, and `espressif8266@4.2.1`.
- Production board: LittleFS reduced to 64 KB (`eagle.flash.1m64.ld`).

### Removed

- `include/wifi.h` and `include/wifi.h.example`: the Wi‑Fi network is chosen from the portal and the OTA password comes from an environment variable.

## v1.0

First tagged version: the firmware written in 2021, documented and tested.

- Web remote for the 8 commands (Default, Auto, Low, Medium, High, Max 15/30/60 min) and `/mode`, served from LittleFS.
- README with photos, wiring, bill of materials, enclosure STL and PCB Gerber files, Home Assistant `rest_command` example, troubleshooting.
- Unit tests on the computer (`pio test -e native`) and GitHub Actions CI.
- Interface in English (`Automatique` became `Auto`), small HTML and CSS fixes.
