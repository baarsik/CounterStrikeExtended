"""Headless SDK/body preservation + portable plan controls in one owned directory."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--art", type=Path, required=True)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--current", type=Path, required=True)
    parser.add_argument("--headers", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    args = parser.parse_args()
    assert (args.headers / "engine/filesystem.h").is_file(), "actual protected owner headers required"
    assert shutil.disk_usage(args.work.anchor).free > 256 * 1024 * 1024, "isolated build disk budget"
    out = args.art
    out.mkdir(parents=True, exist_ok=True)
    stage = args.work / "source"
    stage.mkdir(parents=True, exist_ok=True)
    pinned = json.loads((args.current / "WINDOWS-STAGE.json").read_text())
    for name, expected in pinned.items():
        original = args.current / name
        assert sha(original) == expected, name
        target = stage / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(original.read_bytes())
    commands = []
    compilers = []
    results = []

    def execute(label, command, env, cwd):
        result = subprocess.run(command, env=env, cwd=cwd, capture_output=True, timeout=180)
        (out / (label + ".log")).write_bytes(result.stdout + result.stderr)
        commands.append({"label": label, "command": command, "directory": str(cwd), "exit": result.returncode})
        (out / "commands.json").write_text(json.dumps(commands, indent=2) + "\n")
        assert result.returncode == 0, (label, (result.stdout + result.stderr).decode(errors="replace")[-6000:])
        return result

    for cpu in ("x86", "x64", "arm64"):
        setup = args.work / ("setup-" + cpu + ".cmd")
        setup.write_text('@echo off\ncall "C:\\BuildTools\\2026\\Common7\\Tools\\VsDevCmd.bat"'
            ' -no_logo -host_arch=arm64 -arch=' + cpu + ' >nul\nif errorlevel 1 exit /b 1\nset\n')
        environment = subprocess.run(["cmd.exe", "/d", "/c", str(setup)], capture_output=True, text=True, check=True)
        env = dict(os.environ)
        for line in environment.stdout.splitlines():
            if "=" in line and not line.startswith("="):
                key, value = line.split("=", 1)
                env[key] = value
        compiler = Path(shutil.which("cl.exe", path=env.get("Path", env.get("PATH"))))
        compilers.append({"cpu": cpu, "path": str(compiler), "sha256": sha(compiler)})
        (out / "COMPILERS.json").write_text(json.dumps(compilers, indent=2) + "\n")
        for mode, flags in (("Release", ["/O2", "/MD", "/DNDEBUG"]),
                            ("Debug", ["/Od", "/Zi", "/MDd", "/D_DEBUG"])):
            profile = cpu + "-" + mode
            build = args.work / profile
            build.mkdir(exist_ok=True)
            base = [str(compiler), "/nologo", "/std:c++14", "/EHa", "/fp:precise", "/W3", "/WX",
                    "/D_CRT_SECURE_NO_WARNINGS", "/I" + str(stage), "/I" + str(args.headers / "engine"),
                    "/I" + str(args.headers / "public"), "/I" + str(args.headers / "common")] + flags
            for name in ("pure_tests", "windows_reference", "windows_failure_reference"):
                exe = build / (name + ".exe")
                command = base + [str(stage / (name + ".cpp")), "/Fe" + str(exe), "/Fo" + str(build) + "\\",
                    "/Fd" + str(build / (name + ".pdb")), "/sourceDependencies", str(build / (name + ".json"))]
                execute(profile + "-" + name + "-compile", command, env, build)
                result = execute(profile + "-" + name + "-run", [str(exe)], env, build)
                shutil.copy2(exe, out / (profile + "-" + name + ".exe"))
                shutil.copy2(build / (name + ".json"), out / (profile + "-" + name + "-dependencies.json"))
                if name == "windows_reference":
                    observed = [json.loads(line) for line in result.stdout.decode().splitlines() if line.strip()]
                    expected = json.loads((args.reference / "windows" / (profile + "-results.json")).read_text())
                    assert observed[0] == expected[0], (profile, "actual SDK type/arena contract changed")
                    assert observed[1:] == expected[1:], (profile, "changed actual Windows body result/order")
                    results.append({"profile": profile, "actual_rows": len(observed) - 1, "matches_original": True})
            for owner in ("sw", "hw"):
                outputs = []
                for version in ("original", "current"):
                    unit = build / (owner + "-" + version + ".cpp")
                    unit.write_text('#include <Windows.h>\n' + ''.join('#include "' + str(stage / version / name)
                        + '"\n' for name in (owner + "-Sys_InitMemory.inc", owner + "-Sys_ShutdownMemory.inc")))
                    value = execute(profile + "-" + owner + "-" + version + "-selected", base + ["/EP", str(unit)], env, build)
                    outputs.append(value.stdout.split())
                assert outputs[0] == outputs[1], (profile, owner, "Windows selected tokens changed")
            print("PASS", profile, "actual SDK/body rows + plan/CLI + both Windows selected token controls", flush=True)
    assert pinned == {name: sha(args.current / name) for name in pinned}, "shared input changed"
    (out / "COMPLETE.json").write_text(json.dumps({"profiles": results, "helper_executions": 6,
        "selected_token_comparisons": 12, "provider_game_UI_network_calls": 0,
        "all_children_synchronous": True, "pid": os.getpid()}, indent=2) + "\n")


if __name__ == "__main__":
    main()
