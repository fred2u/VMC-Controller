# Passes the passwords to the firmware from environment variables, so no
# password is stored in the project:
#
#   VMC_OTA_PASSWORD    updates over Wi-Fi with PlatformIO (ArduinoOTA).
#                       Without it, the firmware builds without OTA.
#   VMC_HTTP_PASSWORD   web interface and HTTP routes, user "admin", and the
#                       web update page. Without it, the routes are open and
#                       the web update page is disabled.
#   VMC_SETUP_PASSWORD  VMC-Setup access point, at least 8 characters.
#                       Without it, the access point is open.
#
#   export VMC_OTA_PASSWORD="my-password"      (PowerShell: $env:VMC_OTA_PASSWORD = "my-password")
#   pio run --target upload
import os
import sys

Import("env")

PASSWORDS = [
    ("VMC_OTA_PASSWORD", "OTA_PASSWORD"),
    ("VMC_HTTP_PASSWORD", "HTTP_PASSWORD"),
    ("VMC_SETUP_PASSWORD", "SETUP_PASSWORD"),
]

for variable, macro in PASSWORDS:
    password = os.environ.get(variable, "")
    if not password:
        continue
    if macro == "SETUP_PASSWORD" and len(password) < 8:
        sys.stderr.write("Error: VMC_SETUP_PASSWORD needs at least 8 characters (Wi-Fi rule)\n")
        env.Exit(1)
    env.Append(CPPDEFINES=[(macro, env.StringifyMacro(password))])
