#!/usr/bin/env python3
"""Qualify the native helpers with real owned filesystem paths and standalone signal scopes."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess


ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent


def ensure_symlink(path, target):
    if path.is_symlink():
        assert path.resolve() == target.resolve(), "fixture alias points outside its expected target"
    else:
        assert not path.exists(), "fixture alias collides with an existing ordinary file"
        path.symlink_to(target)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--commands", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    entry = next(item for item in json.loads(args.commands.read_text()) if item["label"] == "TextConsole-release")
    prefix = entry["args"][:entry["args"].index("-o")]
    receipts = []
    for name, architecture, flags in [
            ("arm64-release", "arm64", ["-O2", "-DNDEBUG"]),
            ("arm64-debug", "arm64", ["-O0", "-g", "-D_DEBUG"]),
            ("arm64-sanitizers", "arm64", ["-O1", "-g", "-fsanitize=address,undefined",
                                           "-fno-sanitize-recover=all", "-fno-omit-frame-pointer"]),
            ("x86_64-release", "x86_64", ["-O2", "-DNDEBUG"]),
            ("x86_64-debug", "x86_64", ["-O0", "-g", "-D_DEBUG"])]:
        directory = output / name
        directory.mkdir(exist_ok=True)
        common = [value for value in prefix if not value.startswith("-std=") and value not in ["-O2", "-O0"]]
        common[common.index("-arch") + 1] = architecture
        common += ["-std=c++14"] + flags
        objects = []
        for label, source in [("helpers", ROOT / "GoldSrc-SDK/HLTV/Console/PosixSystem.cpp"),
                              ("fixture", HERE / "helper_tests.cpp"),
                              ("tokens", ROOT / "GoldSrc-SDK/common/TokenLine.cpp")]:
            obj = directory / (label + ".o")
            command = common + ["-c", str(source), "-o", str(obj)]
            result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            (directory / (label + "-compile.log")).write_text(shlex.join(command) + "\n\n" + result.stdout)
            assert result.returncode == 0, result.stdout
            objects.append(str(obj))
        binary = directory / "helpers"
        command = ["/usr/bin/clang++", "-arch", architecture] + flags + objects + ["-o", str(binary)]
        result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (directory / "link.log").write_text(shlex.join(command) + "\n\n" + result.stdout)
        assert result.returncode == 0, result.stdout
        for case in ["argv", "clock", "signals"]:
            result = subprocess.run([str(binary), case], capture_output=True, text=True, timeout=5)
            (directory / (case + ".log")).write_text(result.stdout + result.stderr)
            assert result.returncode == 0, result.stderr
            receipts.append({"profile": name, "case": case, "passed": True})
        runtime = directory / "runtime space Zażółć"
        working = directory / "working space"
        aliases = directory / "aliases"
        for path in [runtime, working / "relative", aliases]:
            path.mkdir(parents=True, exist_ok=True)
        for path in [runtime / "proxy.dll", runtime / "absolute.dll", working / "proxy.dll",
                     working / "relative/path.dll"]:
            path.write_text("ordinary placeholder, never loaded as executable code\n")
        alias = working / "relative/alias.dll"
        ensure_symlink(alias, working / "relative/path.dll")
        executable = runtime / "helper executable"
        shutil.copy2(binary, executable)
        shortcut = aliases / "helper alias"
        ensure_symlink(shortcut, executable)
        for label, path in [("actual-executable-path", executable), ("symlink-executable-path", shortcut)]:
            original = os.getcwd()
            result = subprocess.run([str(path), "paths", str(runtime)], cwd=working,
                                    capture_output=True, text=True, timeout=5)
            (directory / (label + ".log")).write_text(result.stdout + result.stderr)
            assert result.returncode == 0, result.stderr
            assert "EXEDIR " + str(runtime.resolve()) + "\n" in result.stdout, result.stdout
            assert os.getcwd() == original
            receipts.append({"profile": name, "case": label, "passed": True})
        print(name, "5 helper groups passed", flush=True)
    (output / "receipt.json").write_text(json.dumps({"cases": receipts, "command_inventory_sha256":
        hashlib.sha256(args.commands.read_bytes()).hexdigest()}, indent=2) + "\n")


if __name__ == "__main__":
    main()
