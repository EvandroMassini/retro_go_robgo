"""Portable build entry point. Uses the known ESP-IDF toolchain; never flashes implicitly."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def environment():
    env = os.environ.copy()
    config = ROOT / "tools" / "idf.local.json"
    local = json.loads(config.read_text(encoding="utf-8-sig")) if config.exists() else {}
    for key in ("IDF_PATH", "IDF_TOOLS_PATH", "IDF_PYTHON_ENV_PATH"):
        if local.get(key):
            env[key] = local[key]
    if not env.get("IDF_PATH"):
        raise RuntimeError("Ative o ESP-IDF 4.4.8 ou copie tools/idf.local.example.json para tools/idf.local.json e ajuste os caminhos.")
    idf = Path(env["IDF_PATH"])
    python = local.get("python")
    if not python and env.get("IDF_PYTHON_ENV_PATH"):
        python = str(Path(env["IDF_PYTHON_ENV_PATH"]) / ("Scripts/python.exe" if os.name == "nt" else "bin/python"))
    python = python or sys.executable
    exported = subprocess.check_output([python, str(idf / "tools/idf_tools.py"), "export", "--format", "key-value"], env=env, text=True)
    for line in exported.splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            if key.replace("_", "").isalnum():
                for name, current in env.copy().items():
                    value = value.replace("%" + name + "%", current).replace("${" + name + "}", current)
                env[key] = value
    env["PATH"] = str(Path(python).parent) + os.pathsep + str(idf / "tools") + os.pathsep + env.get("PATH", "")
    env.update(IDF_TARGET="esp32", RG_TOOL_TARGET="robgo-rg", RG_TOOL_APPS="launcher fmsx", IDF_CCACHE_ENABLE="0")
    version = subprocess.check_output([python, str(idf / "tools/idf.py"), "--version"], env=env, text=True)
    if "v4.4.8" not in version:
        raise RuntimeError("Esta integracao requer ESP-IDF 4.4.8; encontrado: " + version.strip())
    print(version.strip(), flush=True)
    return python, env


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("check", "build", "upload"))
    parser.add_argument("--port")
    parser.add_argument("--baud", default="460800")
    args = parser.parse_args()
    python, env = environment()
    if args.action == "check":
        subprocess.run([python, str(Path(env["IDF_PATH"]) / "tools/check_python_dependencies.py")], env=env, check=True)
        compiler = shutil.which("xtensa-esp32-elf-gcc", path=env["PATH"])
        if not compiler:
            raise RuntimeError("Compilador Xtensa ausente; instale as ferramentas do ESP-IDF.")
        subprocess.run([compiler, "--version"], env=env, check=True)
        return
    out = ROOT / "dist"
    complete = out / "robgo-completo.bin"
    if args.action == "upload":
        if not args.port or not complete.exists():
            raise RuntimeError("Compile antes e informe --port COMx (ou /dev/ttyUSBx).")
        subprocess.run([python, "-m", "esptool", "--chip", "esp32", "--port", args.port, "--baud", args.baud,
                        "write_flash", "0x0", str(complete)], env=env, check=True)
        return
    if not env.get("PROJECT_VER"):
        try:
            env["PROJECT_VER"] = subprocess.check_output(["git", "describe", "--tags", "--abbrev=5", "--dirty", "--always"], cwd=ROOT, text=True).strip()
        except (OSError, subprocess.CalledProcessError):
            env["PROJECT_VER"] = "robgo-dev"
    subprocess.run([python, "rg_tool.py", "--target", "robgo-rg", "build-img", "launcher", "fmsx"], cwd=ROOT, env=env, check=True)
    image = ROOT / ("retro-go_" + env["PROJECT_VER"] + "_robgo-rg.img").lower()
    data = image.read_bytes()
    entries = [("launcher", 0x10000, 0x100000), ("fmsx", 0x110000, 0xa0000)]
    # Verify the merged bytes, not just the presence of stale output filenames.
    boot = (ROOT / "launcher/build/bootloader/bootloader.bin").read_bytes()
    if data[0x1000:0x1000 + len(boot)] != boot or data[0x8000:0x8002] != b"\xaa\x50":
        raise RuntimeError("Bootloader/tabela ausente na imagem combinada.")
    for app, offset, limit in entries:
        binary = (ROOT / app / "build" / (app + ".bin")).read_bytes()
        if len(binary) > limit or data[offset:offset + len(binary)] != binary:
            raise RuntimeError("Tamanho/offset incorreto: " + app)
    out.mkdir(exist_ok=True)
    shutil.copyfile(image, complete)
    firmware = out / "firmware.bin"
    shutil.copyfile(ROOT / "fmsx/build/fmsx.bin", firmware)
    shutil.copyfile(ROOT / "fmsx/build/fmsx.elf", out / "firmware.elf")
    app_data = firmware.read_bytes()
    app_manifest = {"version": env["PROJECT_VER"], "file": firmware.name,
                    "bytes": len(app_data), "sha256": hashlib.sha256(app_data).hexdigest(),
                    "format": "ESP32 application only", "symbols": "firmware.elf",
                    "note": "No bootloader or partition table. Installation depends on the receiving bootloader."}
    (out / "firmware.json").write_text(json.dumps(app_manifest, indent=2) + "\n", encoding="utf-8")
    for app, _, _ in entries:
        shutil.copyfile(ROOT / app / "build" / (app + ".elf"), out / (app + ".elf"))
    manifest = {"version": env["PROJECT_VER"], "target": "robgo-rg", "idf": "4.4.8", "flash_address": "0x0",
                "file": complete.name, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(),
                "applications": [{"name": name, "offset": hex(offset), "partition_bytes": size} for name, offset, size in entries]}
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print("Imagem completa: " + str(complete), flush=True)


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, subprocess.CalledProcessError) as exc:
        print("Erro: " + str(exc), file=sys.stderr)
        sys.exit(1)
