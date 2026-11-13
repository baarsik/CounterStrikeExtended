#!/usr/bin/env python3
"""Causal controls for the explicitly enabled candidate; no host UI or providers."""
import argparse
import hashlib
import json
from pathlib import Path
import resource
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    here = Path(__file__).resolve().parent
    source = root / "src/common/TextEntryClipboard.cpp"
    original = source.read_text()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    flags = ["/usr/bin/clang++", "-std=c++14", "-arch", "arm64", "-mmacosx-version-min=14.0",
             "-O0", "-g", "-fexceptions", "-fno-fast-math", "-ffp-contract=off", "-Wall", "-Wextra", "-Werror",
             "-DCSE_TEXT_ENTRY_USE_PTHREAD_KEY=1", "-I" + str(root / "src/common"), "-I" + str(here)]
    hidden = ["-Wl,-unexported_symbol,_" + name for name in [
        "pthread_key_create", "pthread_key_delete", "pthread_getspecific", "pthread_setspecific",
        "pthread_mutex_lock", "pthread_mutex_unlock", "pthread_mutex_destroy"]]
    controls = [
        ("proof-bypass", "if (!proof.isHeld(proof.context, proof.moduleIdentity)) {", "if (false) {",
         "guard", [], "QuiescenceRequired"),
        ("equal-reentry", "logicalRemainder >= frame->logicalRemainder_", "logicalRemainder > frame->logicalRemainder_",
         "guard", [], "!equal.Entered()"),
        ("destructor-code-escape", "pthread_key_create(&nativeRegistry.key, nullptr)",
         "pthread_key_create(&nativeRegistry.key, +[](void*) {})", "failure", ["zero"], "nonnullDestructors == 0"),
        ("key-zero-sentinel", "if (nativeRegistry.keyCreated) {",
         "if (nativeRegistry.keyCreated && nativeRegistry.key != pthread_key_t{}) {",
         "failure", ["zero"], "!retired.keyOwned"),
        ("post-retire-admission", "if (nativeRegistry.phase.load(std::memory_order_acquire) != RegistryPhase::Open) {",
         "if (false) {", "failure", ["zero"], "controlled OS trap: access to destroyed mutex"),
        ("active-frame-retirement", "if (nativeRegistry.activeFrames != 0) {", "if (false) {",
         "guard", [], "NativeFrameRetireResult::Busy"),
    ]
    rows = []
    for name, before, after, fixture, arguments, needle in controls:
        assert before in original, name
        modified = original.replace(before, after, 1)
        directory = out / name
        directory.mkdir(exist_ok=True)
        mutated = directory / "TextEntryClipboard.cpp"
        mutated.write_text(modified)
        executable = directory / "control"
        command = flags + [str(here / (fixture + "_tests.cpp"))]
        if fixture == "failure":
            command += [str(here / "os_hooks.cpp")]
        command += [str(mutated)] + hidden + ["-o", str(executable)]
        built = subprocess.run(command, capture_output=True, timeout=120)
        (directory / "compile.log").write_bytes(built.stdout + built.stderr)
        assert built.returncode == 0, (name, built.stderr[-5000:])

        def no_core():
            resource.setrlimit(resource.RLIMIT_CORE, (0, 0))

        tested = subprocess.run([str(executable)] + arguments, capture_output=True, timeout=30, preexec_fn=no_core)
        (directory / "run.log").write_bytes(tested.stdout + tested.stderr)
        assert tested.returncode != 0 and needle.encode() in tested.stderr, (name, tested.stderr[-5000:])
        rows.append({"name": name, "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
                     "mutation": {"before": before, "after": after, "occurrences_replaced": 1},
                     "mutant_sha256": hashlib.sha256(mutated.read_bytes()).hexdigest(),
                     "compile": command, "compile_exit": built.returncode,
                     "run": [str(executable)] + arguments, "run_exit": tested.returncode,
                     "required_failure": needle, "controlled_mutant_detected": True})
        (out / "results.json").write_text(json.dumps(rows, indent=2) + "\n")
    print("PASS six actual-helper causal controls; production source unchanged")


if __name__ == "__main__":
    main()
