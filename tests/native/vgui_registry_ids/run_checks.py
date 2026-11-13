#!/usr/bin/env python3
"""Standalone, actual-header registry checks; builds only under the requested artifact directory."""

import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess


ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
SDK = ROOT / "src"
TABLE_CPP = SDK / "vgui2/src/LocalizedStringTable.cpp"


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def extract_method(source, marker):
    start = source.index(marker)
    opening = source.index("{", start)
    depth = 1
    closing = opening + 1
    while depth:
        depth += (source[closing] == "{") - (source[closing] == "}")
        closing += 1
    return source[start:closing]


def prepare_methods(output):
    source_bytes = TABLE_CPP.read_bytes()
    source = source_bytes.decode("latin-1")
    table_markers = [
        "bool CLocalizedStringTable::SymLess(",
        "const char* CLocalizedStringTable::GetNameByIndex(",
        "wchar_t* CLocalizedStringTable::GetValueByIndex(",
        "StringIndex_t CLocalizedStringTable::FindIndex(",
        "StringIndex_t CLocalizedStringTable::GetFirstStringIndex(",
        "StringIndex_t CLocalizedStringTable::GetNextStringIndex(",
    ]
    legacy_markers = [
        "OldStringIndex_t CLocalizedStringTable_VGUI_Localize002::FindIndex(",
        "const char* CLocalizedStringTable_VGUI_Localize002::GetNameByIndex(",
        "wchar_t* CLocalizedStringTable_VGUI_Localize002::GetValueByIndex(",
        "OldStringIndex_t CLocalizedStringTable_VGUI_Localize002::GetFirstStringIndex(",
        "OldStringIndex_t CLocalizedStringTable_VGUI_Localize002::GetNextStringIndex(",
        "void CLocalizedStringTable_VGUI_Localize002::SetValueByIndex(",
        "const char* CLocalizedStringTable_VGUI_Localize002::GetFileNameByIndex(",
    ]
    receipts = {"source": str(TABLE_CPP), "source_sha256": sha256(source_bytes), "methods": []}
    for name, markers in [("table", table_markers), ("legacy", legacy_markers)]:
        methods = []
        for marker in markers:
            method = extract_method(source, marker)
            receipts["methods"].append({"marker": marker, "sha256": sha256(method.encode("latin-1"))})
            if name == "table":
                method = method.replace("CLocalizedStringTable::", "ControlledTable::")
            methods.append(method)
        (output / f"retained_{name}_methods.inc").write_text("\n\n".join(methods) + "\n")
    (output / "method-source.json").write_text(json.dumps(receipts, indent=2) + "\n")


def execute(arguments, log, cwd=ROOT):
    result = subprocess.run(arguments, cwd=cwd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    log.write_text(shlex.join(map(str, arguments)) + "\n\n" + result.stdout)
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--phase", required=True, choices=["red", "green"])
    parser.add_argument("--compiler", default="/usr/bin/clang++")
    parser.add_argument("--actual-tu-commands", type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    prepare_methods(output)
    common = [args.compiler, "-std=c++14", "-Wall", "-Wextra", "-Werror", "-Wno-unknown-pragmas",
              "-Werror=shorten-64-to-32", "-Werror=pointer-to-int-cast", "-I" + str(SDK), "-I" + str(output)]
    results = []
    for mode, flags in [("release", ["-O2", "-DNDEBUG"]), ("debug", ["-O0", "-g", "-D_DEBUG"]),
                        ("sanitizers", ["-O1", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"])]:
        for order in ["canonical-first", "cs-first"]:
            directories = [SDK / "vgui2/src/public", SDK / "cstrike/public"]
            if order == "cs-first":
                directories.reverse()
            includes = ["-I" + str(path) for path in directories + [SDK / "public"]]
            for entry in ["canonical", "engine", "cs"]:
                name = f"{mode}-{order}-{entry}"
                defines = [] if entry == "canonical" else [f"-DCSE_LOCALIZE_{entry.upper()}=1"]
                command = common + flags + includes + defines + [str(HERE / "localize_probe.cpp"), "-o", str(output / name)]
                compiled = execute(command, output / f"{name}-{args.phase}-compile.log")
                run = None if compiled.returncode else execute([str(output / name)], output / f"{name}-{args.phase}-run.log")
                results.append({"name": name, "compile": compiled.returncode, "run": None if run is None else run.returncode})
        name = f"{mode}-registry"
        command = common + flags + ["-I" + str(SDK / "vgui2/src/public"), "-I" + str(SDK / "public"),
                                   str(HERE / "registry_tests.cpp"), "-o", str(output / name)]
        compiled = execute(command, output / f"{name}-{args.phase}-compile.log")
        run = None if compiled.returncode else execute([str(output / name)], output / f"{name}-{args.phase}-run.log")
        results.append({"name": name, "compile": compiled.returncode, "run": None if run is None else run.returncode})
        name = f"{mode}-hud"
        directories = ["cstrike/cl_dll", "cstrike/dlls", "cstrike/common", "cstrike/public", "common", "engine",
                       "pm_shared", "game_shared", "public", "vgui2/src/public"]
        command = common + flags + ["-I" + str(SDK / path) for path in directories]
        command += [str(HERE / "hud_probe.cpp"), "-o", str(output / name)]
        compiled = execute(command, output / f"{name}-{args.phase}-compile.log")
        run = None if compiled.returncode else execute([str(output / name)], output / f"{name}-{args.phase}-run.log")
        results.append({"name": name, "compile": compiled.returncode, "run": None if run is None else run.returncode})

    if args.actual_tu_commands:
        for entry in json.loads(args.actual_tu_commands.read_text()):
            command = entry["args"][:]
            command[command.index("-o") + 1] = str(output / (entry["name"] + ".o"))
            compiled = execute(command, output / f"{entry['name']}-{args.phase}-whole-tu.log", Path(entry["cwd"]))
            results.append({"name": entry["name"] + "-whole-tu", "compile": compiled.returncode})
    (output / f"results-{args.phase}.json").write_text(json.dumps(results, indent=2) + "\n")
    for result in results:
        print(json.dumps(result))
    if args.phase == "green" and any(result["compile"] or result.get("run", 0) for result in results):
        return 1
    if args.phase == "red" and not any(result["compile"] for result in results):
        raise SystemExit("Expected the unchanged LP64 headers to fail the 32-bit contracts")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
