# Passes the OTA password to the firmware from the VMC_OTA_PASSWORD
# environment variable, so no password is stored in the project:
#
#   export VMC_OTA_PASSWORD="my-password"
#   pio run --target upload
#
# Without the variable, the firmware builds without OTA (USB updates only).
import os

Import("env")

password = os.environ.get("VMC_OTA_PASSWORD", "")
if password:
    env.Append(CPPDEFINES=[("OTA_PASSWORD", env.StringifyMacro(password))])
