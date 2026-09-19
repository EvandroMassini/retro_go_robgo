"""PlatformIO front end for the existing, version-pinned IDF build."""
import os
import subprocess
import sys
from SCons.Script import AlwaysBuild, Default, DefaultEnvironment

env = DefaultEnvironment()
root = env.subst("$PROJECT_DIR")
script = os.path.join(root, "tools", "build_robgo.py")


def invoke(action):
    def run(source, target, env):
        cmd = [sys.executable, script, action]
        if action == "upload":
            port = env.subst("$UPLOAD_PORT") or env.GetProjectOption("upload_port", "")
            if not port:
                print("Defina upload_port no platformio.ini ou use pio run -t upload --upload-port COMx")
                return 1
            cmd += ["--port", port, "--baud", str(env.GetProjectOption("upload_speed", 460800))]
        return subprocess.call(cmd, cwd=root)
    return run


build = env.Alias("buildprog", [], invoke("build"))
AlwaysBuild(build)
Default(build)
env.AddCustomTarget("check", None, invoke("check"), title="Check ESP-IDF", description="Check installed ESP-IDF 4.4.8 tools")
env.AddCustomTarget("upload", build, invoke("upload"), title="Upload complete image", description="Flash launcher + fMSX at 0x0")
