"""Compile actual effects registration and selected engine bodies with unrelated dependencies stubbed."""
from pathlib import Path
import re
import sys

root, output = map(Path, sys.argv[1:])
selected = {
    "cl_tent.cpp": ["CL_TempEntPrepare", "CL_TempEntAlloc", "CL_TempEntAllocNoModel",
                    "CL_TempEntAllocHigh", "R_RicochetSprite", "R_SparkEffect", "R_TempSprite",
                    "CL_TempEntUpdate"],
    "r_part.cpp": ["R_SparkStreaks", "R_AllocParticle", "R_FreeDeadParticles", "R_BeamSetup", "R_BeamDraw",
                   "R_DrawParticles", "R_EntityParticles"],
    "cdll_int.cpp": ["CL_AddVisibleEntity"],
    "mathlib.cpp": ["Length", "VectorMA", "VectorScale"],
}
names = {name for group in selected.values() for name in group}
parts = [re.search(r'^#define MAX_TEMP_ENTITIES .*$',
                   (root / "src/engine/cl_tent.cpp").read_text(), re.M)[0]]
parts.append(re.search(r'^#define NOISE_DIVISIONS .*$',
                       (root / "src/engine/r_part.cpp").read_text(), re.M)[0])
for array in ["ramp1", "ramp2", "ramp3", "gSparkRamp"]:
    parts.append(re.findall(r'^int ' + array + r'\[.*?;',
                           (root / "src/engine/r_part.cpp").read_text(), re.M)[-1])
for filename, functions in selected.items():
    text = (root / "src/engine" / filename).read_text()
    masked = re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',
                    lambda match: " " * len(match[0]), text, flags=re.S)
    for name in functions:
        matches = list(re.finditer(r'^(?:static )?[\w* ]+\b' + name + r'\s*\([^;]*?\)\s*\{', masked, re.M))
        # r_part.cpp has inherited software/hardware copies; qualify each body independently.
        if filename != "r_part.cpp" and len(matches) != 1:
            raise ValueError(f"{filename}: ambiguous/missing {name}: {len(matches)}")
        start = matches[-1].start()
        opening = masked.index("{", matches[-1].start())
        depth, end = 1, opening + 1
        while depth:
            depth += (masked[end] == "{") - (masked[end] == "}")
            end += 1
        parts.append(text[start:end])
prototypes = (root / "src/engine/r_efx.cpp").read_text().split("efx_api_t efx", 1)[0]
for match in re.finditer(r'^(\w+\s*\*?)\s+(\w+)\s*\([^;]*?\);', prototypes, re.M):
    result, name = match.groups()
    if name not in names:
        parts.append(match[0][:-1] + (" { return; }" if result == "void" else " { return {}; }"))
output.write_text("// Generated production bodies; do not edit.\n" + "\n\n".join(parts) + "\n")
