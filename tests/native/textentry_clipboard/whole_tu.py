"""Compile each real Mac TextEntry consumer under its captured owning recipe."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--art", type=Path, required=True)
parser.add_argument("--repo", type=Path, required=True)
args = parser.parse_args()
out = args.art / "whole-mac"
out.mkdir(exist_ok=True)
rows = []
for mode in ["release", "debug"]:
    database = args.art.parent / ("macos-runtime-policy-" + mode) / "compile_commands.json"
    entries = [e for e in json.loads(database.read_text())
               if e["file"].endswith("/vgui2/controls/TextEntry.cpp")
               or e["file"].endswith("/common/TextEntryClipboard.cpp")]
    assert len(entries) == 12
    for entry in entries:
        target = next(p[:-4] for p in Path(entry["output"]).parts if p.endswith(".dir"))
        role = "helper" if entry["file"].endswith("/common/TextEntryClipboard.cpp") else "caller"
        tag = target + "-" + mode + "-" + role
        command = []
        skip = False
        for token in shlex.split(entry["command"]):
            if skip:
                skip = False
                continue
            if token == "-o":
                skip = True
            elif token not in ["-c", entry["file"]]:
                command.append(token)
        command += ["-MD", "-MF", str(out / (tag + ".d")), "-c", entry["file"], "-o", str(out / (tag + ".o"))]
        result = subprocess.run(command, cwd=entry["directory"], capture_output=True, timeout=120)
        (out / (tag + ".log")).write_bytes(result.stdout + result.stderr)
        rows.append({"target": target, "mode": mode, "role": role, "command": command, "exit": result.returncode,
                     "owning_database_sha256": hashlib.sha256(database.read_bytes()).hexdigest(),
                     "Cxx_exception_unwind": "-fexceptions" in command and "-fno-exceptions" not in command})
        print(tag, result.returncode, flush=True)
        (out / "commands.json").write_text(json.dumps(rows, indent=2) + "\n")
assert all(r["exit"] == 0 for r in rows)
