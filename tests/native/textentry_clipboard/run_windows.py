"""Isolated console-only MSVC controls; protected producing sources stay read-only."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
from case_list import CASES

parser = argparse.ArgumentParser()
parser.add_argument("--art", type=Path, required=True)
parser.add_argument("--work", type=Path, required=True)
parser.add_argument("--headers", type=Path, required=True)
args = parser.parse_args()
art = args.art.resolve()
work = args.work.resolve()
work.mkdir(parents=True, exist_ok=True)
outputs = art / "windows"
outputs.mkdir(exist_ok=True)
sdk = args.headers / "GoldSrc-SDK"
stage = work / "source"
for name, expected in json.loads((art / "PRODUCTION_FREEZE.json").read_text())["files"].items():
    source = art / "production-source" / name
    assert hashlib.sha256(source.read_bytes()).hexdigest() == expected
    target = stage / name
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)
tests = work / "tests"
tests.mkdir(exist_ok=True)
for name in ["widget_tests.cpp", "vpanel_binding.cpp", "helper_tests.cpp", "unexpected_binding.cpp"]:
    shutil.copy2(art / "windows-source" / name, tests / name)
recipes = json.loads((art / "windows-owning-ninja-contexts.json").read_text())
rows = []
commands = []
compiler_records = []


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fields(recipe):
    return dict(line.strip().split(" = ", 1) for line in recipe["block"][1:] if " = " in line)


def recipe_for(cpu, mode, target):
    directory = ("runtime-windows-x64-" + mode.lower()) if cpu != "x86" else mode.lower()
    matches = [r for r in recipes if Path(r["build"]).name == directory
               and ("targets\\" + target + "\\") in r["block"][0]]
    assert len(matches) == 1, (cpu, mode, target)
    return matches[0]


def flags_for(cpu, mode, target):
    data = fields(recipe_for(cpu, mode, target))
    return [token for key in ["DEFINES", "FLAGS", "INCLUDES"]
            for token in shlex.split(data[key], posix=False)]


def execute(command, env, directory, label):
    result = subprocess.run(command, env=env, cwd=directory, capture_output=True, timeout=180)
    (outputs / (label + ".log")).write_bytes(result.stdout + result.stderr)
    commands.append({"label": label, "command": command, "exit": result.returncode})
    (outputs / "commands.json").write_text(json.dumps(commands, indent=2) + "\n")
    if result.returncode:
        print("FAIL", label, (result.stdout + result.stderr).decode(errors="replace")[-6500:], flush=True)
    return result


for cpu in ["x86", "x64", "arm64"]:
    setup = work / ("setup-" + cpu + ".cmd")
    setup.write_text('@echo off\ncall "C:\\BuildTools\\2026\\Common7\\Tools\\VsDevCmd.bat" '
                     '-no_logo -host_arch=arm64 -arch=' + cpu + ' >nul\nif errorlevel 1 exit /b 1\nset\n')
    result = subprocess.run(["cmd.exe", "/d", "/c", setup], capture_output=True, text=True, check=True)
    env = dict(os.environ)
    for line in result.stdout.splitlines():
        if "=" in line and not line.startswith("="):
            key, value = line.split("=", 1)
            env[key] = value
    compiler = Path(shutil.which("cl.exe", path=env.get("Path", env.get("PATH"))))
    linker = Path(shutil.which("link.exe", path=env.get("Path", env.get("PATH"))))
    compiler_records.append({"cpu": cpu, "compiler": str(compiler), "sha256": sha(compiler),
                             "flags_origin": "actual V17 Ninja target recipes" if cpu != "arm64"
                             else "component ARM64 profile reuses x64 source flags; no owning ARM64 build claimed"})
    (outputs / "compilers.json").write_text(json.dumps(compiler_records, indent=2) + "\n")
    for mode in ["Release", "Debug"]:
        tag = cpu + "-" + mode
        build = work / tag
        build.mkdir(exist_ok=True)
        vpanel_flags = flags_for(cpu, mode, "vgui2")
        vpanel_objects = []
        for source, label in [(sdk / "vgui2/src/VPanel.cpp", "VPanel"),
                              (tests / "vpanel_binding.cpp", "VPanelBinding")]:
            obj = build / (label + ".obj")
            command = [str(compiler), "/nologo"] + vpanel_flags + ["/c", str(source), "/Fo" + str(obj),
                        "/Fd" + str(build / (label + ".pdb"))]
            assert execute(command, env, build, tag + "-" + label).returncode == 0
            vpanel_objects.append(str(obj))
        for owner, target in [("common", "gameui"), ("cs", "cs_client")]:
            fixture = build / owner
            fixture.mkdir(exist_ok=True)
            incoming = art / "windows-source" / owner
            for source in incoming.iterdir():
                if source.suffix not in [".cpp", ".inc"]:
                    continue
                text = source.read_text(encoding="latin1")
                mac_root = "/Users/mmalakhouski/Documents/GitHub/CounterStrikeExtended/GoldSrc-SDK"
                text = text.replace(mac_root + "/common/TextEntryClipboard.h",
                                    (stage / "GoldSrc-SDK/common/TextEntryClipboard.h").as_posix())
                text = text.replace(mac_root, sdk.as_posix())
                if source.name == "controlled_services.inc":
                    old = "std::int32_t GetTimeMillis() override"
                    assert text.count(old) == 1
                    text = text.replace(old, "long GetTimeMillis() override", 1)
                (fixture / source.name).write_text(text, encoding="latin1")
            flags = flags_for(cpu, mode, target)
            flags += ["/I" + str(fixture), "/I" + str(stage / "GoldSrc-SDK/common")]
            objects = list(vpanel_objects)
            for source, label in [(tests / "widget_tests.cpp", "widget"),
                                  (tests / "unexpected_binding.cpp", "reject"),
                                  (fixture / "actual_clipboard_helper.cpp", "helper")]:
                obj = fixture / (label + ".obj")
                dependency = fixture / (label + "-dependencies.json")
                command = [str(compiler), "/nologo"] + flags + ["/c", str(source), "/Fo" + str(obj),
                            "/Fd" + str(fixture / (label + ".pdb")), "/sourceDependencies", str(dependency)]
                assert execute(command, env, fixture, tag + "-" + owner + "-" + label).returncode == 0
                shutil.copy2(dependency, outputs / (tag + "-" + owner + "-" + label + "-dependencies.json"))
                objects.append(str(obj))
            executable = fixture / "widget-tests.exe"
            command = [str(linker), "/NOLOGO", "/INCREMENTAL:NO", "/OUT:" + str(executable)] + objects
            assert execute(command, env, fixture, tag + "-" + owner + "-link").returncode == 0
            for test in CASES:
                result = execute([str(executable), test], env, fixture, tag + "-" + owner + "-" + test)
                rows.append({"cpu": cpu, "mode": mode, "owner": owner, "test": test, "exit": result.returncode})
                assert result.returncode == 0, (tag, owner, test)
            (outputs / "results.json").write_text(json.dumps(rows, indent=2) + "\n")
            print("PASS", tag, owner, len(CASES), "actual-class controls", flush=True)
        helper_obj = build / "pure-helper.obj"
        helper_exe = build / "pure-helper.exe"
        flags = ["/nologo", "/std:c++14", "/EHsc", "/W4", "/WX", "/fp:precise", "/I" + str(stage / "GoldSrc-SDK/common")]
        flags += ["/O2", "/MD", "/DNDEBUG"] if mode == "Release" else ["/Od", "/Zi", "/MDd", "/D_DEBUG"]
        command = [str(compiler)] + flags + [str(tests / "helper_tests.cpp"),
                    str(stage / "GoldSrc-SDK/common/TextEntryClipboard.cpp"), "/Fe" + str(helper_exe),
                    "/Fo" + str(build) + "\\", "/Fd" + str(build / "pure.pdb")]
        assert execute(command, env, build, tag + "-pure-helper-build").returncode == 0
        assert execute([str(helper_exe)], env, build, tag + "-pure-helper-run").returncode == 0
(outputs / "EXIT.txt").write_text("0\n")
print("PASS Windows isolated caller matrix", len(rows), "actual-widget cases", flush=True)
