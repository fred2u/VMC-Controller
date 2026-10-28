# Builds the firmware version into the firmware, from the Git tags:
#
#   v1.1                  built exactly on the v1.1 tag
#   v1.1-3-g1a2b3c4       3 commits after v1.1 (commit 1a2b3c4)
#   v1.1-3-g1a2b3c4-dirty same, with changes that are not committed yet
#   unknown               Git or the project history is not available
#
# The ESP prints it on the serial port at boot, answers it on /version and
# shows it at the bottom of the web interface. See "Releases" in the README.
import subprocess

Import("env", "projenv")


def git_version():
    try:
        out = subprocess.run(
            ["git", "describe", "--tags", "--always", "--dirty"],
            cwd=env.subst("$PROJECT_DIR"),
            capture_output=True,
            text=True,
            timeout=10,
        )
    except (OSError, subprocess.SubprocessError):
        return ""
    return out.stdout.strip() if out.returncode == 0 else ""


version = git_version() or "unknown"
print("VMC Controller firmware version: " + version)

# Only for the project's own code: libraries are not rebuilt on every commit.
projenv.Append(CPPDEFINES=[("VMC_VERSION", env.StringifyMacro(version))])
