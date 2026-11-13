"""Query the owning A3D CMake target, including generated geometry sections."""
from pathlib import Path
import json
import re
import sys

sys.dont_write_bytecode = True
root = Path(sys.argv[1]).resolve()
source = Path(sys.argv[2]).resolve()
config = sys.argv[3]
arch = sys.argv[4]
output = Path(sys.argv[5])
sys.path.insert(0, str(root / "tools"))
from cmake_inventory import query_target

evaluated = query_target(root, "a3dapi", configuration=config, target_os="windows", architecture=arch)
original = root / "src/a3dapi"


def mapped(path):
    path = Path(path)
    return source / path.name if path.parent == original else path


def quote(value):
    return '"' + str(value).replace("\\", "/").replace('"', '\\"') + '"'


items = [item for item in evaluated["sources"]
         if item["language"] in ("C", "CXX", "RC") or Path(item["path"]).suffix.lower() == ".rc"]
paths = []
settings = []
for item in items:
    if "generated" in item:
        path = output.parent / Path(item["path"]).name
        path.write_text("#define " + item["generated"]["section"] + '\n#include "'
                        + str(source / "A3dGeom.cpp").replace("\\", "/") + '"\n')
    else:
        path = mapped(item["path"])
    if not path.is_file():
        raise ValueError(f"Selected A3D source is missing after query mapping: {path}")
    paths.append(path)
    definitions = item["compile_definitions"]
    includes = [mapped(v) for v in item["include_directories"]]
    options = item["compile_options"]
    for key, values in [("COMPILE_DEFINITIONS", definitions), ("INCLUDE_DIRECTORIES", includes),
                        ("COMPILE_OPTIONS", options)]:
        if values:
            settings.append("set_property(SOURCE " + quote(path) + " APPEND PROPERTY " + key + " "
                            + " ".join(quote(v) for v in values) + ")")

definition = source / "a3dapi.def"
if arch != "x86":
    # Fixture-only native C export projection: preserve names, ordinals, PRIVATE.
    # Root production export-policy integration is a separate requirement.
    text = definition.read_text(encoding="cp1252")
    text = re.sub(r"(?m)^(\s*)(\w+)=_\2@\d+(\s+@\d+\s+PRIVATE)\s*$", r"\1\2\3", text)
    assert "=_" not in text
    definition = output.parent / "native-a3dapi.def"
    definition.write_text(text)
paths.append(definition)
lines = ["set(CSE_A3D_MODULE_SOURCES"] + ["  " + quote(p) for p in paths] + [")"] + settings
lines += ["set(CSE_A3D_MODULE_INCLUDE " + quote(source) + ")"]
libraries = evaluated["link_libraries"]
lines += ["set(CSE_A3D_MODULE_LIBRARIES " + " ".join(quote(v) for v in libraries) + ")"]
output.write_text("\n".join(lines) + "\n")
output.with_suffix(".json").write_text(json.dumps(evaluated, indent=2) + "\n")
text = (source / "A3d3.cpp").read_text(encoding="cp1252")
first = text.index("static const CLSID s_A3dDalClassId")
(output.parent / "factory_ids.inc").write_text(text[first:text.index("CA3dClassFactory::CA3dClassFactory", first)])
