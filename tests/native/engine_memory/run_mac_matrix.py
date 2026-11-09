"""Actual extracted heap functions and pure helpers; no game/provider execution."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def functions(text):
    result = {}
    for name in ("Sys_InitMemory", "Sys_ShutdownMemory"):
        position = 0
        for owner in ("sw", "hw"):
            start = text.index("void " + name + "()", position)
            end = text.index("\n}\n", start) + 3
            position = end
            result[owner + "-" + name + ".inc"] = text[start:end]
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--art", type=Path, required=True)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--contexts", type=Path, required=True)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[3]
    tests = Path(__file__).resolve().parent
    out = args.art.resolve()
    out.mkdir(parents=True, exist_ok=True)
    stage = out / "source"
    stage.mkdir(exist_ok=True)
    commands = []

    def execute(label, command, expected=0, cwd=repo, environment=None):
        result = subprocess.run(command, cwd=cwd, env=environment, capture_output=True, timeout=180)
        (out / (label + ".log")).write_bytes(result.stdout + result.stderr)
        commands.append({"label": label, "command": command, "directory": str(cwd), "exit": result.returncode})
        (out / "commands.json").write_text(json.dumps(commands, indent=2) + "\n")
        if expected == 0:
            assert result.returncode == 0, (label, (result.stdout + result.stderr).decode(errors="replace")[-6000:])
        else:
            assert result.returncode != 0, label
        return result

    source = repo / "GoldSrc-SDK/engine/sys_dll2.cpp"
    helper = repo / "GoldSrc-SDK/common/NativeEngineMemory.h"
    for name, text in functions(source.read_text()).items():
        (stage / name).write_text(text)
    # Independent expected bytes are captured from the actual Windows SDK/body
    # executions, never recomputed from the candidate helper.
    for name in ("cases.inc", "golden.inc", "COM_CheckParm.inc"):
        (stage / name).write_bytes((args.reference / name).read_bytes())
    inputs = [source, helper, repo / "GoldSrc-SDK/common/legacy_text_to_integer.h",
              repo / "GoldSrc-SDK/engine/host.h"] + sorted(tests.glob("*.*"))
    inputs = [path for path in inputs if path.is_file()]
    before = {str(path): sha(path) for path in inputs}
    (out / "INPUTS.json").write_text(json.dumps(before, indent=2) + "\n")
    compiler = Path("/usr/bin/clang++")
    (out / "COMPILER.json").write_text(json.dumps({"path": str(compiler), "sha256": sha(compiler),
        "version": subprocess.check_output([str(compiler), "--version"], text=True)}, indent=2) + "\n")
    base = [str(compiler), "-std=c++14", "-arch", "arm64", "-mmacosx-version-min=14.0",
            "-Wall", "-Wextra", "-Werror", "-Wshorten-64-to-32", "-ffp-model=precise", "-ffp-contract=off",
            "-Wno-overriding-option",
            "-fexceptions", "-I" + str(repo / "GoldSrc-SDK/common"),
            "-I" + str(repo / "GoldSrc-SDK/engine"), "-I" + str(repo / "GoldSrc-SDK/public"),
            "-I" + str(tests), "-I" + str(stage)]
    profiles = {"Release": ["-O2", "-DNDEBUG"], "Debug": ["-O0", "-g", "-D_DEBUG"],
                "Sanitizers": ["-O1", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]}
    for profile, flags in profiles.items():
        for test in ("pure_tests", "query_tests", "actual_body_tests"):
            tag = profile + "-" + test
            exe = out / tag
            execute(tag + "-compile", base + flags + ["-MD", "-MF", str(out / (tag + ".d")),
                    str(tests / (test + ".cpp")), "-o", str(exe)])
            env = dict(os.environ)
            # Apple ASan explicitly rejects LeakSanitizer. Arena ownership is
            # checked through actual release and ASan; no LSan result is claimed.
            env["ASAN_OPTIONS"] = "detect_leaks=0:halt_on_error=1"
            env["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
            execute(tag + "-run", [str(exe)], environment=env)
    contexts = json.loads(args.contexts.read_text())
    for item in contexts:
        command = item["command"]
        # Existing context is already an exact -fsyntax-only owning-TU command.
        execute("whole-" + item["label"], command, expected=1, cwd=Path(item["entry"]["directory"]))
        command = [str(tests / "actual_body_tests.cpp") if value == str(source) else value for value in command]
        command += ["-I" + str(tests), "-I" + str(stage)]
        execute("extracted-" + item["label"], command, cwd=Path(item["entry"]["directory"]))
    # Token equality is a preprocessing control, not a substitute for the actual
    # SDK Windows compilation/execution performed by run_windows_component.py.
    comparisons = []
    for owner in ("sw", "hw"):
        outputs = []
        for version, folder in (("original", args.reference), ("current", stage)):
            unit = out / (owner + "-" + version + "-other-posix.cpp")
            unit.write_text(''.join('#include "' + name + '"\n' for name in
                (owner + "-Sys_InitMemory.inc", owner + "-Sys_ShutdownMemory.inc")))
            result = execute(owner + "-" + version + "-other-posix-preprocess", [str(compiler), "-E", "-P",
                "-x", "c++", "-U__APPLE__", "-U_WIN32", "-I" + str(folder), str(unit)])
            tokens = result.stdout.split()
            outputs.append(tokens)
        assert outputs[0] == outputs[1], owner + " non-Apple selected body changed"
        comparisons.append({"owner": owner, "equal_tokens": True, "token_count": len(outputs[0])})
    (out / "OTHER-POSIX-TOKENS.json").write_text(json.dumps(comparisons, indent=2) + "\n")
    assert before == {str(path): sha(path) for path in inputs}, "source changed while producing matrix"
    (out / "COMPLETE.json").write_text(json.dumps({"profiles": 3, "executions": 9,
        "actual_owner_rows": 116 * 2 * 3, "actual_owning_context_compiles": 4,
        "whole_TU_known_blockers": 4, "max_live_arena_bytes": 134217728,
        "physical_capacity_reserve_bytes": 1073741824, "source_unchanged": True,
        "provider_game_UI_network_calls": 0}, indent=2) + "\n")
    print("PASS Mac R/D/ASan+UBSan, four strict extracted owning contexts; whole-TU blockers retained")


if __name__ == "__main__":
    main()
