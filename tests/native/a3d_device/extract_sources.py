"""Extract unchanged production bodies for a device-boundary component TU."""
import argparse
import hashlib
import json
import re
from pathlib import Path


def function(text, signature):
    start = text.index(signature)
    opening = text.index("{", start)
    depth = 0
    branches = []
    offset = opening
    for line in text[opening:].splitlines(keepends=True):
        directive = line.lstrip()
        if directive.startswith("#if"):
            branches.append([depth, None])
        elif directive.startswith(("#else", "#elif")):
            branches[-1][1] = depth
            depth = branches[-1][0]
        elif directive.startswith("#endif"):
            _, previous = branches.pop()
            assert previous is None or previous == depth, "conditional branches must have equal brace balance"
        else:
            for index, character in enumerate(line):
                depth += (character == "{") - (character == "}")
                if depth == 0:
                    return start, text[start:offset + index + 1]
        offset += len(line)
    raise ValueError(signature)


parser = argparse.ArgumentParser()
parser.add_argument("source", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
blocks = []
receipt = []


def add(name, start, body):
    path = args.source / name
    line = path.read_text(encoding="cp1252").count("\n", 0, start) + 1
    blocks.append(f'#line {line} "{path.as_posix()}"\n{body}\n')
    receipt.append({"file": str(path), "line": line,
                    "body_sha256": hashlib.sha256(body.encode()).hexdigest()})


text = (args.source / "ia3dapi.h").read_text(encoding="cp1252")
for name in ["A3D_1ST_REFLECTIONS", "A3D_DIRECT_PATH_A3D", "A3D_OCCLUSIONS", "A3D_CL_NORMAL",
             "A3D_RESOURCE_MODE_DYNAMIC"]:
    match = re.search(r"^#define " + name + r"\s+[^\n]+", text, re.MULTILINE)
    add("ia3dapi.h", match.start(), match.group())
for name in ["A3DERROR_CODE", "A3DERROR_FAILED_INIT", "A3DERROR_FEATURE_NOT_SUPPORTED"]:
    match = re.search(r"^#define " + name + r"(?:\([^\n]+?\))?\s+[^\n]+", text, re.MULTILINE)
    add("ia3dapi.h", match.start(), match.group())
text = (args.source / "A3d3.h").read_text(encoding="cp1252")
start = text.index("enum\n")
end = text.index("\n", text.index("#define A3D_COMPAT_LEGACY_MARKER_VALUE", start))
add("A3d3.h", start, text[start:end])
text = (args.source / "resman.h").read_text(encoding="cp1252")
start = text.index("class IA3dDal :")
add("resman.h", start, text[start:text.index("class ResMan :", start)])
text = (args.source / "A3dScene.cpp").read_text(encoding="cp1252")
for name in ["A3D_BACKEND_INITIALIZATION_MODE", "A3D_BACKEND_COMPATIBILITY_ID"]:
    match = re.search(r"^#define " + name + r"\s+[^\n]+", text, re.MULTILINE)
    add("A3dScene.cpp", match.start(), match.group())
start = text.index("typedef struct a3d_backend_caps_s")
add("A3dScene.cpp", start, text[start:text.index("HRESULT CA3d::SetupBackend", start)])
blocks.append('#include "fixture_support.h"\n')
geometry = (args.source / "A3dGeom.cpp").read_text(encoding="cp1252")
for signature in ["void* CA3dObjectList::Next()", "void CA3dObjectList::Clear()"]:
    add("A3dGeom.cpp", *function(geometry, signature))
if "static HRESULT InitializeNativeA3dDevice(" in text:
    helper_start, helper_body = function(text, "static HRESULT InitializeNativeA3dDevice(")
    guard_start = text.rfind("#if UINTPTR_MAX > UINT32_MAX", 0, helper_start)
    guard_end = text.index("\n#endif", helper_start + len(helper_body)) + len("\n#endif")
    add("A3dScene.cpp", guard_start, text[guard_start:guard_end])
for signature in ["HRESULT CA3d::SetupBackend(", "HRESULT CA3d::ShutdownBackend()",
                  "HRESULT STDMETHODCALLTYPE CA3d::Init("]:
    add("A3dScene.cpp", *function(text, signature))
args.output.mkdir(parents=True, exist_ok=True)
(args.output / "production_bodies.inc").write_text("\n".join(blocks))
(args.output / "extracted-bodies.json").write_text(json.dumps(receipt, indent=2) + "\n")
