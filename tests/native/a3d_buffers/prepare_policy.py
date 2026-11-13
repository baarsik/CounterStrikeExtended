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
subjects = {"a2d.cpp", "a2dbuffer.cpp", "native_buffer.cpp", "native_mix.cpp", "native_platform.cpp"}
lines = []
receipt = {}
for item in evaluated["sources"]:
    name = Path(item["path"]).name
    if name not in subjects:
        continue
    options = item["compile_options"]
    receipt[name] = item
    args = " ".join('"' + value.replace('"', '\\"') + '"' for value in options)
    source = root / "src/a3dapi" / name
    lines.append(f'set_property(SOURCE "{source}" PROPERTY COMPILE_OPTIONS {args})')
(out / "subject-policy.cmake").write_text("\n".join(lines) + "\n")
(out / "subject-policy.json").write_text(json.dumps(receipt, indent=2) + "\n")
