"""Use the retained manifest's actual POSIX options for directly compiled subjects."""
import json
from pathlib import Path
import sys

root = Path(sys.argv[1]).resolve()
config = sys.argv[2]
out = Path(sys.argv[3]).resolve()
sys.dont_write_bytecode = True
sys.path.insert(0, str(root / "tools"))
import translate_msbuild as translator

manifest = root / "GoldSrc-SDK/projects/vs2026/a3dapi.vcxproj"
evaluated = translator.evaluate(manifest, config)
subjects = {"a2d.cpp", "a2dbuffer.cpp", "native_buffer.cpp", "native_mix.cpp", "native_platform.cpp"}
lines = []
receipt = {}
for item in evaluated["items"]:
    name = Path(item["path"]).name
    if name not in subjects:
        continue
    policy = translator.portable_game_settings(item["metadata"])
    options = policy["common"] + policy["cxx"]
    if config == "Debug" and policy["symbols"]:
        options.append("-g")
    receipt[name] = {"metadata": item["metadata"], "options": options}
    args = " ".join('"' + value + '"' for value in options)
    source = root / "GoldSrc-SDK/a3dapi" / name
    lines.append(f'set_property(SOURCE "{source}" PROPERTY COMPILE_OPTIONS {args})')
(out / "subject-policy.cmake").write_text("\n".join(lines) + "\n")
(out / "subject-policy.json").write_text(json.dumps(receipt, indent=2) + "\n")
