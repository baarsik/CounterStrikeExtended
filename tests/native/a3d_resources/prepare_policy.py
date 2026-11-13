"""Use the owning CMake target's actual POSIX options for directly compiled subjects."""
import json
from pathlib import Path
import sys

root = Path(sys.argv[1]).resolve()
config = sys.argv[2]
out = Path(sys.argv[3]).resolve()
sys.dont_write_bytecode = True
sys.path.insert(0, str(root / "tools"))
from cmake_inventory import query_target

evaluated = query_target(root, "a3dapi", configuration=config, target_os="macos", architecture="arm64")
lines = []
receipt = {}
for item in evaluated["sources"]:
    name = Path(item["path"]).name
    if item["language"] != "CXX":
        continue
    options = item["compile_options"]
    receipt[name] = item
    args = " ".join('"' + value.replace('"', '\\"') + '"' for value in options)
    source = out / "generated" / name if name.startswith("geometry_") else root / "src/a3dapi" / name
    definitions = " ".join('"' + value.replace('"', '\\"') + '"' for value in item["compile_definitions"])
    if definitions:
        lines.append(f'set_property(SOURCE "{source}" APPEND PROPERTY COMPILE_DEFINITIONS {definitions})')
    lines.append(f'set_property(SOURCE "{source}" PROPERTY COMPILE_OPTIONS {args})')
(out / "subject-policy.cmake").write_text("\n".join(lines) + "\n")
(out / "subject-policy.json").write_text(json.dumps(receipt, indent=2) + "\n")
