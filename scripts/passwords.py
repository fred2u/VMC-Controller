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
# And the optional MQTT broker (Home Assistant discovery):
#
#   VMC_MQTT_HOST       broker name or IP address. Without it, no MQTT.
#   VMC_MQTT_PORT       default 1883
#   VMC_MQTT_USER, VMC_MQTT_PASSWORD   if the broker asks for them
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
    ("VMC_MQTT_HOST", "MQTT_HOST"),
    ("VMC_MQTT_USER", "MQTT_USER"),
    ("VMC_MQTT_PASSWORD", "MQTT_PASSWORD"),
]

for variable, macro in PASSWORDS:
    password = os.environ.get(variable, "")
    if not password:
        continue
    if macro == "SETUP_PASSWORD" and len(password) < 8:
        sys.stderr.write("Error: VMC_SETUP_PASSWORD needs at least 8 characters (Wi-Fi rule)\n")
        env.Exit(1)
    env.Append(CPPDEFINES=[(macro, env.StringifyMacro(password))])

port = os.environ.get("VMC_MQTT_PORT", "")
if port:
    if not port.isdigit():
        sys.stderr.write("Error: VMC_MQTT_PORT must be a number\n")
        env.Exit(1)
    env.Append(CPPDEFINES=[("MQTT_PORT", int(port))])
