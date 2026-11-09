#!/usr/bin/env python3
"""Standalone, unselected owner component. Named boards only; no General API."""
import argparse
import hashlib
import json
import pathlib
import resource
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=pathlib.Path, required=True)
    arguments = parser.parse_args()
    root = pathlib.Path(__file__).resolve().parents[3]
    output = arguments.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    compiler = pathlib.Path("/Library/Developer/CommandLineTools/usr/bin/clang++")
    common = [str(compiler), "-std=c++14", "-arch", "arm64", "-mmacosx-version-min=14.0",
              "-isysroot", "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk",
              "-fexceptions", "-Wall", "-Wextra", "-Werror", "-I", str(root / "GoldSrc-SDK/vgui2/src")]
    objc = ["-fobjc-arc", "-fobjc-exceptions", "-fobjc-arc-exceptions"]
    rows = []

    def run(name, command, expected=0, needle=None):
        def no_core():
            resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        result = subprocess.run(command, cwd=root, capture_output=True, text=True, timeout=60,
                                preexec_fn=no_core if expected != 0 else None)
        (output / (name + ".log")).write_text(result.stdout + result.stderr)
        rows.append({"name": name, "command": command, "exit": result.returncode,
                     "expected": expected, "required_failure_text": needle})
        (output / "commands.json").write_text(json.dumps(rows, indent=2) + "\n")
        assert (result.returncode == 0 if expected == 0 else result.returncode != 0), (name, result.stderr)
        assert needle is None or needle in result.stderr, (name, result.stderr)
        return result

    for profile, options in [("Release", ["-O2"]), ("Debug", ["-O0", "-g"]),
                             ("Sanitizer", ["-O1", "-g", "-fsanitize=address,undefined",
                                            "-fno-omit-frame-pointer"])]:
        directory = output / profile
        directory.mkdir(exist_ok=True)
        flags = common + options
        production_flags = flags + ["-fvisibility=hidden"] + (["-DNDEBUG"] if profile == "Release" else [])

        def compile_object(name, source, production=True, extra=None):
            target = directory / (name + ".o")
            selected = production_flags if production else flags
            command = selected + (objc if source.endswith(".mm") else []) + (extra or [])
            command += ["-c", str(root / source), "-o", str(target)]
            run(profile + "-compile-" + name, command)
            return str(target)

        core = compile_object("core", "GoldSrc-SDK/vgui2/src/NativeClipboard.cpp")
        owner = compile_object("owner", "GoldSrc-SDK/vgui2/src/NativeClipboardOwner.cpp")
        backend = compile_object("backend", "GoldSrc-SDK/vgui2/src/NativeClipboardMac.mm")
        pure_test = compile_object("pure-test", "tests/native/clipboard_owner/owner_tests.cpp", False)
        named_test = compile_object("named-test", "tests/native/clipboard_owner/named_tests.mm", False)
        for name, objects, libraries in [("pure", [pure_test, core, owner], []),
                                          ("named", [named_test, core, owner, backend], ["-framework", "AppKit"])]:
            executable = str(directory / name)
            run(profile + "-link-" + name, flags + objects + libraries + ["-o", executable])
            run(profile + "-run-" + name, [executable])

        host_object = compile_object("host", "tests/native/clipboard_owner/module_host.cpp", False)
        host = str(directory / "module-host")
        run(profile + "-link-host", flags + [host_object, "-o", host])
        fixtures = [compile_object("fixture" + str(tag), "tests/native/clipboard_owner/module_fixture.cpp",
                                   extra=["-DFIXTURE_TAG=" + str(tag)]) for tag in [1, 2]]
        mac_fixtures = [compile_object("mac-fixture" + str(tag), "tests/native/clipboard_owner/module_fixture.cpp",
                                       extra=["-DFIXTURE_TAG=" + str(tag), "-DFIXTURE_REAL_MAC=1"])
                        for tag in [1, 2]]
        for binding, linker_flags in [("two-level", []), ("flat", ["-Wl,-flat_namespace"])]:
            images = []
            for tag, fixture in zip([1, 2], fixtures):
                image = str(directory / (binding + "-" + str(tag) + ".dylib"))
                run(profile + "-link-" + binding + str(tag), flags + ["-dynamiclib"] + linker_flags
                    + [fixture, core, owner, "-o", image])
                symbols = run(profile + "-exports-" + binding + str(tag), ["/usr/bin/nm", "-gU", image])
                exported = [line.split()[-1] for line in symbols.stdout.splitlines() if line.strip()]
                assert all(name.startswith("_Fixture") for name in exported), exported
                images.append(image)
            run(profile + "-run-" + binding, [host] + images)
            if profile == "Debug" and binding == "two-level":
                run("negative-counter-only", [host] + images + ["counter-only"], -1,
                    "counter-only pin did not retain real images")
                run("negative-immediate-unload", [host] + images + ["unsafe-immediate"], -1,
                    "final code reference released while component is executing")
            mac_images = []
            for tag, fixture in zip([1, 2], mac_fixtures):
                image = str(directory / ("mac-" + binding + "-" + str(tag) + ".dylib"))
                run(profile + "-link-mac-" + binding + str(tag), flags + ["-dynamiclib"] + linker_flags
                    + [fixture, core, owner, backend, "-framework", "AppKit", "-o", image])
                mac_images.append(image)
            run(profile + "-observe-mac-dylib-" + binding, [host] + mac_images + ["actual-mac-observe"])
            bundle_images = []
            for tag, fixture in zip([1, 2], mac_fixtures):
                image = str(directory / ("diagnostic-bundle-" + binding + "-" + str(tag) + ".bundle"))
                run(profile + "-link-diagnostic-bundle-" + binding + str(tag), flags + ["-bundle"] + linker_flags
                    + [fixture, core, owner, backend, "-framework", "AppKit", "-o", image])
                bundle_images.append(image)
            run(profile + "-run-diagnostic-bundle-" + binding, [host] + bundle_images + ["actual-mac"])

    (output / "compiler.json").write_text(json.dumps({
        "path": str(compiler), "sha256": hashlib.sha256(compiler.read_bytes()).hexdigest(),
        "version": subprocess.check_output([str(compiler), "--version"], text=True),
        "policy": "Standalone C++14 component; per-source ARC+Sync flags. No owning target/manifests selected.",
        "pure_and_named_profiles": 3, "pure_independent_dylib_runs": 6, "negative_controls": 2,
        "actual_ObjC_dylib_residency_observations": 6, "actual_ObjC_dylib_unload_qualified": False,
        "same_source_diagnostic_bundle_unload_runs": 6, "production_format_changed": False,
        "general_factory": False, "windows_isolation_executed": False,
        "publisher_ready": False, "cold_load_ready": False}, indent=2) + "\n")
    print("PASS: pure/named/loader controls; ObjC dylib residency OBSERVED, unload remains unqualified")


if __name__ == "__main__":
    main()
