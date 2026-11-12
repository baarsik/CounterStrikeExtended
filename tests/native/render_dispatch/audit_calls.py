#!/usr/bin/env python3
"""Bounded T2a source routing audit. This is not the full 25-owner T2 acceptance audit."""
import argparse
import json
from pathlib import Path
import re
import sys

OWNERS = ("gl_rmain.cpp", "r_triangle.cpp", "glHud.cpp")
# A short, explicit set for this slice; texture/list/array/shader/FBO owners remain later work.
T2A_RAW = {"glEnable", "glFinish", "glFogf", "glFogfv", "glFogi"}


def raw_calls(text):
    # Strip comments/string literals so documentation and resolver names are not calls.
    text = re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', "", text, flags=re.S)
    return re.findall(r"\b(?:gl[A-Z][A-Za-z0-9_]*|SDL_GL_[A-Za-z0-9_]+)\s*\(", text)


def audit(root):
    errors = []
    rows = []
    for owner in OWNERS:
        path = root / "GoldSrc-SDK/engine" / owner
        text = path.read_text()
        calls = [entry.split("(")[0].strip() for entry in raw_calls(text)]
        for name in calls:
            errors.append(f"{owner}: unowned raw call {name}")
        rows.append({"owner": owner, "raw_calls": calls, "scope": "T2a only"})
    qgl = (root / "GoldSrc-SDK/engine/qgl.cpp").read_text()
    for required in ("InstallLegacyDispatch", "RemoveLegacyDispatch"):
        if required not in qgl:
            errors.append("qgl.cpp: missing " + required)
    aliases = (root / "GoldSrc-SDK/engine/gl_vidnt.cpp").read_text()
    for name in ("glActiveTextureARB", "glMultiTexCoord2fARB", "glSelectTextureSGIS", "glMTexCoord2fSGIS"):
        if not re.search(r'RouteLegacyExtension\s*\(\s*"' + re.escape(name) + r'"', aliases):
            errors.append("gl_vidnt.cpp: unowned alias " + name)
    return rows, errors


def self_test():
    assert raw_calls("// glBegin(ignored)\nCseRenderBegin(GL_TRIANGLES); /* glEnd() */") == []
    assert raw_calls('const char* name = "glEnable(ignored)";') == []
    # Unit negative injects a new raw call into a temporary source string.
    source = "void Producer() { CseRenderEnable(GL_FOG); }"
    assert not raw_calls(source)
    assert raw_calls(source.replace("CseRenderEnable", "glEnable")) == ["glEnable("]
    assert raw_calls("void Producer() { SDL_GL_SwapWindow(window); }") == ["SDL_GL_SwapWindow("]
    print("PASS T2a audit injected-raw negative (source data only)")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path("."))
    parser.add_argument("--inventory", type=Path)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return 0
    if args.inventory and not args.inventory.is_file():
        parser.error("inventory is missing")
    rows, errors = audit(args.root)
    print(json.dumps({"slice": "T2a", "full_T2_complete": False, "owners": rows, "errors": errors}, indent=2))
    return bool(errors)


if __name__ == "__main__":
    sys.exit(main())
