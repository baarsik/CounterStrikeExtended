"""Contained semantic mutants execute actual caller bodies and real owner types."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--baseline", type=Path, required=True)
parser.add_argument("--out", type=Path, required=True)
args = parser.parse_args()
args.out.mkdir(parents=True, exist_ok=True)
base_command = json.loads((args.baseline / "build-command.json").read_text())["command"]
mutants = [
    ("copy-count", "actual_textentry.inc", "system()->SetClipboardText(clipboard.Data(), clipboard.Length());",
     "system()->SetClipboardText(clipboard.Data(), selectionLength);", "copy", "CopySelected must publish"),
    ("restore-capacity", "actual_textentry.inc", "system()->SetClipboardText(clipboard.Data(), clipboard.Length());",
     "system()->SetClipboardText(clipboard.Data(), bufferSize);", "tail", "restore logical cells"),
    ("remainder-off-one", "actual_textentry.inc", "clipboard.Length() - index);", "clipboard.Length() - index - 1);",
     "map", "chain remaining text"),
    ("missing-initialization", "actual_clipboard_helper.cpp", "std::memset(data_, 0, bytes);", "",
     "no-write", "exact actual-method API"),
    ("bounded-unlimited", "actual_textentry.inc", "clipboard.Read(bufferSize, completeText, readClipboard)",
     "clipboard.Read(bufferSize, (static_cast<void>(completeText), false), readClipboard)",
     "grow", "complete underestimated"),
    ("equal-reentry", "actual_clipboard_helper.cpp", "logicalRemainder >= frame->logicalRemainder_",
     "logicalRemainder > frame->logicalRemainder_", "cycle", "controlled no-progress recursion stop"),
    ("null-target", "actual_textentry.inc", "target != 0 && target != GetVPanel()", "true", "null",
     "invalid controlled route"),
    ("missing-message-delete", "actual_textentry.inc", "message->deleteThis();", "static_cast<void>(message);",
     "map", "synchronous message caller owns deletion"),
    ("bounded-extra-cell", "actual_clipboard_helper.cpp", "candidate.length_ = capacity - 1;",
     "candidate.length_ = capacity;", "max-long", "exact actual-method API"),
    ("NUL-count", "actual_clipboard_helper.cpp", "candidate.length_ = logicalLength;",
     "candidate.length_ = count;", "tail", "tail text"),
]
rows = []
for name, file, before, after, test, diagnostic in mutants:
    stage = args.out / name
    stage.mkdir(exist_ok=True)
    for source in args.baseline.iterdir():
        if source.suffix in [".inc", ".cpp", ".o"]:
            shutil.copy2(source, stage / source.name)
    source = stage / file
    text = source.read_text(encoding="latin1")
    # Restore and Copy contain the same setter; mutate the intended method only.
    if name == "copy-count":
        begin = text.index("void TextEntry::CopySelected()")
        prefix, suffix = text[:begin], text[begin:]
        assert suffix.count(before) == 1
        text = prefix + suffix.replace(before, after, 1)
    elif name == "restore-capacity":
        assert text.count(before) == 2
        text = text.replace(before, after, 1)
    else:
        assert text.count(before) == 1, name
        text = text.replace(before, after, 1)
    source.write_text(text, encoding="latin1")
    command = [word.replace(str(args.baseline), str(stage)) for word in base_command]
    compiled = subprocess.run(command, capture_output=True)
    (stage / "compile.log").write_bytes(compiled.stdout + compiled.stderr)
    assert compiled.returncode == 0, (name, compiled.stderr[-2500:])
    result = subprocess.run([str(stage / "widget-tests"), test], capture_output=True)
    (stage / "run.log").write_bytes(result.stdout + result.stderr)
    assert result.returncode == 1 and diagnostic.encode() in result.stderr, (name, result.stdout, result.stderr)
    rows.append({"mutant": name, "test": test, "command": command,
                 "compile_exit": compiled.returncode, "runtime_exit": result.returncode,
                 "expected_diagnostic": diagnostic, "controlled_actual_owner_gate_rejected": True})
    print("REJECT", name, flush=True)
(args.out / "results.json").write_text(json.dumps(rows, indent=2) + "\n")
