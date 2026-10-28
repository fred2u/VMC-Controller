# Adds the OTA password (VMC_OTA_PASSWORD) to uploads over Wi-Fi only.
#
# Runs after the platform builder, which has chosen the uploader by then:
# espota.py for an IP address or a .local name, esptool.py for USB. The
# password is only passed to espota.py, so USB uploads stay unchanged.
# There is no need to set PLATFORMIO_UPLOAD_FLAGS.
import os

Import("env")

password = os.environ.get("VMC_OTA_PASSWORD", "")
if password and str(env.get("UPLOADER", "")).endswith("espota.py"):
    env.Append(UPLOADERFLAGS=["--auth=" + password])
