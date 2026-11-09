"""Prepare exact leaf/body inputs and derive the mixer policy from its manifest."""
from pathlib import Path
import hashlib
import json
import sys

sys.dont_write_bytecode = True
root = Path(sys.argv[1]).resolve()
source = Path(sys.argv[2]).resolve()
config = sys.argv[3]
architecture = sys.argv[4]
output = Path(sys.argv[5]).resolve()
sys.path.insert(0, str(root / "tools"))
import translate_msbuild as translator

evaluated = translator.evaluate(root / "GoldSrc-SDK/projects/vs2026/a3dapi.vcxproj", config)
item = next(item for item in evaluated["items"] if Path(item["path"]).name == "a2d.cpp")
metadata = item["metadata"]
assert metadata["Optimization"] == "MaxSpeed"
assert metadata["FloatingPointModel"] == "Precise"
assert metadata["IntrinsicFunctions"] == "true"
options = ["/O2", "/fp:precise", "/Oi"]
if architecture == "x86" and metadata["EnableEnhancedInstructionSet"] == "NoExtensions":
    options.append("/arch:IA32")
output.mkdir(parents=True, exist_ok=True)
(output / "selected_policy.cmake").write_text("set(CSE_A3D_SUBJECT_OPTIONS " + " ".join(options) + ")\n")
(output / "selected_policy.json").write_text(json.dumps(item, indent=2) + "\n")

geometry = source.parent / "A3dGeom.cpp"
text = geometry.read_text(encoding="cp1252")
start = text.index("DWORD __stdcall Mixer_MultiplyFixed16(")
opening = text.index("{", start)
end = text.index("}", opening) + 1
body = text[start:end]
line = text.count("\n", 0, start) + 1
(output / "multiply_fixed_body.inc").write_text(f'#line {line} "{geometry.as_posix()}"\n' + body + "\n")

text = source.read_text(encoding="cp1252")
declaration = "DWORD __stdcall Mixer_MultiplyFixed16(DWORD left, DWORD right);"
assert text.count('#include "WallEdge.h"') == 1
portable = text.replace('#include "WallEdge.h"', declaration)
(output / "portable_mixer.inc").write_text(f'#line 1 "{source.as_posix()}"\n' + portable)
(output / "prepared-hashes.json").write_text(json.dumps({
    "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
    "multiply_body_sha256": hashlib.sha256(body.encode()).hexdigest(),
    "portable_body_sha256": hashlib.sha256(portable.encode()).hexdigest(),
    "Mac_adapter": "Only unused geometry-class header replaced by its real integer-helper declaration; all mixer bodies/tables retained"
}, indent=2) + "\n")
