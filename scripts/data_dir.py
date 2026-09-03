# PlatformIO pre-script: each env gets its own LittleFS payload directory,
# data/<env>/, retaining PlatformIO's per-environment filesystem layout.
import os
Import("env")
env.Replace(PROJECT_DATA_DIR=os.path.join(env["PROJECT_DIR"], "data", env["PIOENV"]))
