"""Actual whole caller objects under all recorded Win32/x64 owner recipes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--art", type=Path, required=True)
parser.add_argument("--work", type=Path, required=True)
args = parser.parse_args()
args.work.mkdir(parents=True, exist_ok=True)
out = args.art / "whole-windows"
out.mkdir(exist_ok=True)
stage = args.work / "source"
for name, expected in json.loads((args.art / "PRODUCTION_FREEZE.json").read_text())["files"].items():
    data = (args.art / "production-source" / name).read_bytes()
    assert hashlib.sha256(data).hexdigest() == expected
    target = stage / name
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(data)
recipes = json.loads((args.art / "windows-owning-ninja-contexts.json").read_text())
rows = []
for cpu in ["x86", "x64"]:
    setup = args.work / (cpu + "-setup.cmd")
    setup.write_text('@echo off\ncall "C:\\BuildTools\\2026\\Common7\\Tools\\VsDevCmd.bat" '
                     '-no_logo -host_arch=arm64 -arch=' + cpu + ' >nul\nset\n')
    result = subprocess.run(["cmd.exe", "/d", "/c", setup], capture_output=True, text=True, check=True)
    env = dict(os.environ)
    for line in result.stdout.splitlines():
        if "=" in line and not line.startswith("="):
            key, value = line.split("=", 1)
            env[key] = value
    compiler = Path(shutil.which("cl.exe", path=env.get("Path", env.get("PATH"))))
    for mode in ["Release", "Debug"]:
        directory = "runtime-windows-x64-" + mode.lower() if cpu == "x64" else mode.lower()
        matches = [r for r in recipes if Path(r["build"]).name == directory and "VPanel.cpp.obj:" not in r["block"][0]]
        assert len(matches) == 6
        for recipe in matches:
            target = recipe["block"][0].split(" ")[1].split("\\")[1]
            fields = dict(line.strip().split(" = ", 1) for line in recipe["block"][1:] if " = " in line)
            flags = [word for key in ["DEFINES", "FLAGS", "INCLUDES"] for word in shlex.split(fields[key], posix=False)]
            file = "src/cstrike/vgui2/controls/TextEntry.cpp" if target == "cs_client" else "src/vgui2/controls/TextEntry.cpp"
            # Relocation removes the implicit source-directory search. Restore
            # exactly that original directory before the retained /I list.
            original_directory = (Path(recipe["build"]).parent.parent / file).parent
            flags = ["/I" + str(original_directory)] + flags
            tag = cpu + "-" + mode + "-" + target
            build = args.work / tag
            build.mkdir(exist_ok=True)
            for source, label, extra in [(stage / file, "caller", []),
                                         (stage / "src/common/TextEntryClipboard.cpp", "helper", ["/EHsc"])]:
                dependency = build / (label + "-dependencies.json")
                command = [str(compiler), "/nologo"] + flags + extra + ["/c", str(source),
                           "/Fo" + str(build / (label + ".obj")), "/Fd" + str(build / (label + ".pdb")),
                           "/sourceDependencies", str(dependency)]
                result = subprocess.run(command, env=env, cwd=build, capture_output=True, timeout=180)
                (out / (tag + "-" + label + ".log")).write_bytes(result.stdout + result.stderr)
                rows.append({"cpu": cpu, "mode": mode, "target": target, "role": label,
                             "command": command, "exit": result.returncode,
                             "compiler_sha256": hashlib.sha256(compiler.read_bytes()).hexdigest(),
                             "helper_Sync_EH_explicit": label == "helper",
                             "existing_caller_policy_flags_changed": False,
                             "restored_original_implicit_include_directory": str(original_directory)})
                (out / "commands.json").write_text(json.dumps(rows, indent=2) + "\n")
                if result.returncode:
                    print("FAIL", tag, label, (result.stdout + result.stderr).decode(errors="replace")[-5000:], flush=True)
                    raise SystemExit(1)
                shutil.copy2(dependency, out / (tag + "-" + label + "-dependencies.json"))
            print("PASS", tag, "actual whole caller+Sync helper objects", flush=True)
(out / "EXIT.txt").write_text("0\n")
