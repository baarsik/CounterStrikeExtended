#!/usr/bin/env python3
"""Causal controls compiled from current real component, never repo mutations."""
import argparse
import hashlib
import json
import pathlib
import resource
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=pathlib.Path, required=True)
    arguments = parser.parse_args()
    root = pathlib.Path(__file__).resolve().parents[3]
    output = arguments.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    backend = (root / "src/vgui2/src/NativeClipboardMac.mm").read_text()
    owner = (root / "src/vgui2/src/NativeClipboardOwner.cpp").read_text()
    rows = []

    def replaced(source, before, after):
        assert source.count(before) == 1, before
        return source.replace(before, after)

    variants = []
    variants.append(("lost-read-token", "backend", replaced(backend,
        "            const NSInteger after = state.board.changeCount;\n", ""), "read-exception", 79, "lost the read token"))
    name, kind, source, mode, code, needle = variants[-1]
    variants[-1] = (name, kind, replaced(source, "            ++state.activeReads;\n",
        "            ++state.activeReads;\n            const NSInteger after = state.board.changeCount;\n"), mode, code, needle)
    variants.append(("untransferred-factory", "backend", replaced(backend,
        "    if (owner.HoldsRoot()) {\n        return Status::Busy;\n    }\n", ""),
        "repeated-factory", 77, "untransferred backend"))
    variants.append(("rejected-global-cleanup", "backend", replaced(backend,
        "state.board && state.namedIdentityAccepted && !state.releaseSubmitted",
        "state.board && !state.releaseSubmitted"), "rejected-identity", 78, "rejected board identity"))
    source = replaced(backend, 'constexpr char PrivateTypeBytes[] = "org.cse.clipboard.snapshot-v1";\n', "")
    source = replaced(source,
        "            state.privateType = [[NSString alloc] initWithBytes:PrivateTypeBytes length:sizeof(PrivateTypeBytes) - 1\n"
        "                encoding:NSASCIIStringEncoding];",
        '            state.privateType = @"org.cse.clipboard.snapshot-v1";')
    variants.append(("image-backed-type", "backend", source, "type-storage", 76, "component-image object/bytes"))
    variants.append(("coupled-count", "owner", replaced(owner,
        "parsed.unicode = {node->serial, (flags & Raw8Flag) != 0, raw8Bytes,",
        "parsed.unicode = {node->serial, (flags & Raw8Flag) != 0, unicodeBytes,"), None, -1, "CountLegacyText"))
    variants.append(("ignored-generation", "owner", replaced(owner,
        "    if (result == Status::Ok && node->read.before != node->read.after) {\n"
        "        result = Status::OwnershipChanged;\n    }\n", ""), None, -1, "Status::ReleaseFailed"))
    variants.append(("unknown-flags", "owner", replaced(owner,
        "(flags & ~(Raw8Flag | UnicodeFlag)) != 0 || ", ""), None, -1, "Status::MalformedBundle"))
    variants.append(("null-deallocation", "owner", replaced(owner,
        "        if (node->storage) {\n            dependencies.deallocate(dependencies.context, node->storage);\n        }",
        "        dependencies.deallocate(dependencies.context, node->storage);"),
        "null-cleanup", -1, "data != nullptr"))
    variants.append(("zero-allocation", "owner", replaced(owner,
        "    if (result == Status::Ok && node->read.bytes.storageBytes < HeaderBytes) {\n"
        "        result = Status::MalformedBundle;\n    }\n", ""), "empty", -1, "bytes != 0"))

    for name, kind, changed, mode, expected, needle in variants:
        directory = output / name
        directory.mkdir()
        for filename in ["NativeClipboard.h", "NativeClipboard.cpp", "NativeClipboardOwner.h",
                         "NativeClipboardOwner.cpp", "NativeClipboardMac.h", "NativeClipboardMac.mm"]:
            shutil.copyfile(root / "src/vgui2/src" / filename, directory / filename)
        filename = "NativeClipboardMac.mm" if kind == "backend" else "NativeClipboardOwner.cpp"
        (directory / filename).write_text(changed)
        target = directory / "control"
        test = "named_tests.mm" if kind == "backend" else "owner_tests.cpp"
        command = ["/usr/bin/clang++", "-std=c++14", "-O0", "-g", "-mmacosx-version-min=14.0",
                   "-fexceptions", "-Wall", "-Wextra", "-Werror", "-I", str(directory),
                   str(root / "tests/native/clipboard_owner" / test), str(directory / "NativeClipboard.cpp"),
                   str(directory / "NativeClipboardOwner.cpp")]
        if kind == "backend":
            command += [str(directory / "NativeClipboardMac.mm"), "-fobjc-arc", "-fobjc-exceptions",
                        "-fobjc-arc-exceptions", "-framework", "AppKit"]
        command += ["-o", str(target)]
        result = subprocess.run(command, capture_output=True, text=True)
        (directory / "compile.log").write_text(result.stdout + result.stderr)
        assert result.returncode == 0, (name, result.stderr)

        def no_core():
            resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        result = subprocess.run([str(target)] + ([mode] if mode else []), capture_output=True,
                                text=True, timeout=30, preexec_fn=no_core)
        (directory / "run.log").write_text(result.stdout + result.stderr)
        assert result.returncode != 0 and (expected == -1 or result.returncode == expected), name
        assert needle in result.stderr, (name, result.stderr)
        rows.append({"name": name, "command": command, "exit": result.returncode, "expected": expected,
                     "required_failure_text": needle, "changed_source_sha256": hashlib.sha256(changed.encode()).hexdigest(),
                     "classification": "causal semantic control reconstructed from final source, not an original RED source snapshot"})
        (output / "controls.json").write_text(json.dumps(rows, indent=2) + "\n")
    print("PASS: all9 compiled causal controls rejected; production source untouched")


if __name__ == "__main__":
    main()
