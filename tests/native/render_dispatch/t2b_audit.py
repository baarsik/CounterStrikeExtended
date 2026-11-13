#!/usr/bin/env python3
"""File-only T2b routing DATA audit: five pinned owners, 518 historical entries; not T2d."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

LEDGER = Path(__file__).with_name("t2b_owner_ledger.json")
CALL = re.compile(r"\b((?:qgl|gl|CseRender)[A-Z][A-Za-z0-9_]*|SDL_GL_[A-Za-z0-9_]+)\s*\(")
LITERALS = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.S)
RAW_RELEASED = {"glDeleteLists", "glCallList", "glGenLists", "glNewList", "glEndList", "glPixelStorei",
                "glEnable", "glDisable", "glFogi"}


def calls(text):
    stripped = LITERALS.sub(lambda match: "\n" * match.group().count("\n"), text)
    return [(match.group(1), stripped.count("\n", 0, match.start()) + 1) for match in CALL.finditer(stripped)]


def audit_owner(owner, text):
    actual = calls(text)
    errors = []
    expected = owner["calls"]
    if len(actual) != len(expected):
        errors.append(f'{owner["path"]}: expected {len(expected)} calls, observed {len(actual)}')
    for index, (entry, (symbol, line)) in enumerate(zip(expected, actual)):
        original = entry["symbol"]
        required = "CseRender" + original[2:] if original in RAW_RELEASED else original
        if symbol != required:
            errors.append(f'{owner["path"]}:{line}: entry {index}: expected {required}, observed {symbol}')
    return errors


def audit(root):
    ledger = json.loads(LEDGER.read_text())
    errors = []
    rows = []
    total = sum(len(owner["calls"]) for owner in ledger["owners"])
    if total != 518 or len(ledger["owners"]) != 5:
        errors.append("t2b-source-audit: historical ledger extent changed")
    for owner in ledger["owners"]:
        source = (root / owner["path"]).read_text()
        errors.extend(audit_owner(owner, source))
        rows.append({"path": owner["path"], "entries": len(owner["calls"]),
                     "sha256": hashlib.sha256(source.encode()).hexdigest()})
    aliases = (root / "src/engine/gl_vidnt.cpp").read_text()
    for name in ("glBindTextureEXT", "glColorTableEXT"):
        if not re.search(r'RouteLegacyExtension\s*\(\s*"' + re.escape(name) + '"', aliases):
            errors.append("gl_vidnt.cpp: missing T2b alias handoff " + name)
    return {"scope": "T2b five-owner lexical DATA only", "entries": total, "owners": rows,
            "full_T2_complete": False, "compiler_or_runtime_executed": False, "errors": errors}


def self_test(root):
    ledger = json.loads(LEDGER.read_text())
    owner = next(owner for owner in ledger["owners"] if owner["path"].endswith("gl_model.cpp"))
    source = (root / owner["path"]).read_text()
    # Strings are temporary DATA, never compiled; the checked-out producer is not modified.
    routed = re.sub(r"\bglDeleteLists\s*\(", "CseRenderDeleteLists(", source)
    assert not audit_owner(owner, routed)
    injected = routed.replace("CseRenderDeleteLists(", "glDeleteLists(", 1)
    assert audit_owner(owner, injected)
    assert not calls('// glBegin(ignored)\nconst char* text = "glEnd(ignored)";')
    assert calls("void Extra() { glPixelStorei(GL_UNPACK_ROW_LENGTH, 0); }") == [("glPixelStorei", 1)]
    print("PASS T2b raw-call negative in temporary source DATA; no C++/SDK/helper executed")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path("."))
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test(args.root)
        return 0
    result = audit(args.root)
    print(json.dumps(result, indent=2))
    return bool(result["errors"])


if __name__ == "__main__":
    sys.exit(main())
