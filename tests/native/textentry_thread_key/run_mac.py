#!/usr/bin/env python3
"""Explicitly enabled candidate; no production selection or host UI/provider."""
import argparse
import hashlib
import json
from pathlib import Path
import resource
import shlex
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--art-root", type=Path, required=True)
    parser.add_argument("--skip-widgets", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    here = Path(__file__).resolve().parent
    accepted = root / "tests/native/textentry_clipboard"
    sys.path.insert(0, str(accepted))
    from case_list import CASES
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    rows = []
    common = ["/usr/bin/clang++", "-std=c++14", "-arch", "arm64", "-mmacosx-version-min=14.0",
              "-fexceptions", "-fno-fast-math", "-ffp-contract=off", "-Wall", "-Wextra", "-Werror",
              "-DCSE_TEXT_ENTRY_USE_PTHREAD_KEY=1", "-I" + str(root / "GoldSrc-SDK/common"), "-I" + str(here)]
    hidden = ["-Wl,-unexported_symbol,_" + symbol for symbol in [
        "pthread_key_create", "pthread_key_delete", "pthread_getspecific", "pthread_setspecific",
        "pthread_mutex_lock", "pthread_mutex_unlock", "pthread_mutex_destroy"]]

    def execute(label, command, directory=root, expected=0, needle=None):
        def no_core():
            resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        result = subprocess.run(command, cwd=directory, capture_output=True, timeout=120,
                                preexec_fn=no_core if expected else None)
        (out / (label + ".log")).write_bytes(result.stdout + result.stderr)
        rows.append({"label": label, "command": command, "directory": str(directory), "exit": result.returncode,
                     "expected": expected, "required_failure_text": needle})
        (out / "commands.json").write_text(json.dumps(rows, indent=2) + "\n")
        assert (result.returncode == 0 if expected == 0 else result.returncode != 0), (label, result.stderr[-5000:])
        assert needle is None or needle.encode() in result.stderr, (label, result.stderr[-5000:])
        return result

    def strip_recipe(entry):
        result = []
        skip = False
        for token in shlex.split(entry["command"]):
            if skip:
                skip = False
            elif token == "-o":
                skip = True
            elif token not in ["-c", entry["file"]]:
                result.append(token)
        return result

    for profile, options in [("Release", ["-O2"]), ("Debug", ["-O0", "-g"]),
                             ("Sanitizer", ["-O1", "-g", "-fsanitize=address,undefined",
                                            "-fno-omit-frame-pointer"])]:
        directory = out / profile
        directory.mkdir(exist_ok=True)
        flags = common + options
        helper = str(root / "GoldSrc-SDK/common/TextEntryClipboard.cpp")
        for name, sources in [("guard", [str(here / "guard_tests.cpp"), helper]),
                              ("failure", [str(here / "failure_tests.cpp"), str(here / "os_hooks.cpp"), helper])]:
            target = str(directory / name)
            execute(profile + "-compile-" + name, flags + sources + hidden + ["-o", target])
            if name == "guard":
                execute(profile + "-run-guard", [target])
            else:
                for case in ["unused", "zero", "create-again", "create-memory", "set-memory", "lock",
                             "retire-lock", "delete", "destroy"]:
                    execute(profile + "-failure-" + case, [target, case])
                for case, needle in [("restore-fatal", "destructor setspecific"),
                                     ("destructor-lock-fatal", "destructor mutex_lock"),
                                     ("unlock-fatal", "mutex_unlock")]:
                    execute(profile + "-failure-" + case, [target, case], expected=-1, needle=needle)
        host = str(directory / "module-host")
        execute(profile + "-compile-host", flags + [str(here / "modules_test.cpp"), "-o", host])
        for binding, linker in [("twolevel", []), ("flat", ["-flat_namespace"])]:
            images = []
            for tag, name in [(1, "a"), (2, "b")]:
                image = str(directory / (binding + "-" + name + ".dylib"))
                execute(profile + "-compile-" + binding + name, flags + ["-fvisibility=hidden", "-dynamiclib"]
                        + linker + ["-DCSE_KEY_MODULE_TAG=" + str(tag), "-DCSE_KEY_MODULE_API=cse_key_frame_api_" + name,
                                    str(here / "module.cpp"), str(here / "os_hooks.cpp"), helper] + hidden + ["-o", image])
                header = execute(profile + "-header-" + binding + name, ["/usr/bin/otool", "-hv", image])
                sections = execute(profile + "-sections-" + binding + name, ["/usr/bin/otool", "-l", image])
                symbols = execute(profile + "-exports-" + binding + name, ["/usr/bin/nm", "-gU", image])
                assert b"HAS_TLV_DESCRIPTORS" not in header.stdout
                assert b"__thread_vars" not in sections.stdout and b"__objc_" not in sections.stdout
                exported = [line.split()[-1] for line in symbols.stdout.splitlines() if line.strip()]
                assert exported == [("_cse_key_frame_api_" + name).encode()], exported
                images.append(image)
            lifecycle = ["single-lifecycle"] if profile == "Sanitizer" else []
            execute(profile + "-run-" + binding, [host] + images + lifecycle)
            if profile == "Debug" and binding == "twolevel":
                execute("negative-counter-only", [host] + images + ["counter-only"], expected=-1,
                        needle="real code reference released")
                execute("negative-skip-retirement", [host] + images + ["skip-retirement"], expected=-1,
                        needle="key retirement was skipped")

        if args.skip_widgets:
            continue
        mode = "release" if profile == "Release" else "debug"
        original_database = args.art_root / ("macos-runtime-policy-" + mode) / "compile_commands.json"
        entries = json.loads(original_database.read_text())
        candidate = []
        for entry in entries:
            entry = dict(entry)
            if entry["file"].endswith("/TextEntry.cpp") or entry["file"].endswith("/TextEntryClipboard.cpp"):
                entry["command"] += " -DCSE_TEXT_ENTRY_USE_PTHREAD_KEY=1"
            candidate.append(entry)
        database = directory / "candidate-compile-commands.json"
        database.write_text(json.dumps(candidate, indent=2) + "\n")
        (directory / "context-origin.json").write_text(json.dumps({
            "actual_database": str(original_database), "sha256": hashlib.sha256(original_database.read_bytes()).hexdigest(),
            "delta": "isolated candidate private define only; owning FP/EH/options preserved; no production target changed"},
            indent=2) + "\n")
        for owner in ["common", "cs"]:
            fixture = directory / owner
            execute(profile + "-prepare-" + owner, [sys.executable, str(accepted / "prepare_fixture.py"),
                    "--repo", str(root), "--out", str(fixture), "--database", str(database), "--owner", owner])
            context = json.loads((fixture / "owning-context.json").read_text())
            vpanel = root / "GoldSrc-SDK/vgui2/src/VPanel.cpp"
            entry = next(e for e in entries if e["file"] == str(vpanel))
            vflags = strip_recipe(entry)
            sanitize = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"] if profile == "Sanitizer" else []
            objects = []
            for source, name in [(vpanel, "vpanel"), (accepted / "vpanel_binding.cpp", "binding")]:
                obj = str(fixture / (name + ".o"))
                execute(profile + "-" + owner + "-compile-" + name, vflags + sanitize
                        + ["-c", str(source), "-o", obj], Path(entry["directory"]))
                objects.append(obj)
            command = context["flags"] + ["-I" + str(fixture), "-I" + str(here), "-I" + str(accepted),
                      "-I" + str(root / "GoldSrc-SDK/vgui2/src"), str(here / "widget_key_tests.cpp"),
                      str(accepted / "unexpected_binding.cpp"), "-I" + str(root / "GoldSrc-SDK/common"),
                      str(fixture / "actual_clipboard_helper.cpp"), str(here / "os_hooks.cpp"), *objects,
                      "-Wl,-dead_strip", *hidden, *sanitize, "-o", str(fixture / "widget-tests")]
            execute(profile + "-" + owner + "-compile-widget", command, Path(context["entry"]["directory"]))
            for case in CASES + ["key-create", "key-set", "key-lock"]:
                execute(profile + "-" + owner + "-widget-" + case, [str(fixture / "widget-tests"), case])
        if profile != "Sanitizer":
            owning = [e for e in candidate if e["file"].endswith("/vgui2/controls/TextEntry.cpp")
                      or e["file"].endswith("/common/TextEntryClipboard.cpp")]
            assert len(owning) == 12
            for index, entry in enumerate(owning):
                execute(profile + "-whole-" + str(index), strip_recipe(entry) + ["-c", entry["file"], "-o",
                        str(directory / ("whole-" + str(index) + ".o"))], Path(entry["directory"]))
    (out / "coverage-boundary.json").write_text(json.dumps({
        "production_candidate_enabled": False,
        "RD_in_process_lifecycle_rounds": 4,
        "ASan_UBSan_lifecycle_rounds_per_fresh_process": 1,
        "repeated_ASan_reload_qualified": False,
        "reason": "original and SDK-free bare controls reproduce __asan_register_image_globals reload crash",
        "source_or_toolchain_fix_claimed": False,
        "root_quiescence_wiring_implemented": False}, indent=2) + "\n")
    print("PASS bounded candidate gates; repeated ASan reload UNQUALIFIED; private define stays OFF")


if __name__ == "__main__":
    main()
