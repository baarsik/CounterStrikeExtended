"""Compile complete production precache and HW/SW decal-name function bodies."""
from pathlib import Path
import re
import sys


def functions(text, name):
    bodies = []
    pattern = r"^[\w:*<> ]+\b" + re.escape(name) + r"\([^;]*?\)\s*\{"
    for match in re.finditer(pattern, text, re.MULTILINE):
        start = text.index("{", match.start())
        depth = 1
        end = start + 1
        while depth:
            depth += (text[end] == "{") - (text[end] == "}")
            end += 1
        bodies.append(text[match.start():end])
    return bodies


engine = Path(sys.argv[1])
output = Path(sys.argv[2])
client = (engine / "cl_main.cpp").read_text()
parts = functions(client, "CL_IsResourceIndexValid")
assert len(parts) <= 1, "ambiguous resource-index helper"
for name in ("CL_PrecacheResources", "CL_PrecacheBSPModels"):
    bodies = functions(client, name)
    assert len(bodies) == 1, (name, len(bodies))
    parts.extend(bodies)
(output / "precache.inc").write_text("\n\n".join(parts) + "\n")
decals = functions((engine / "decals.cpp").read_text(), "Draw_DecalSetName")
assert len(decals) == 2, "expected both software and hardware decal setters"
(output / "decal_names.inc").write_text(
    "#ifdef SW_RENDERER\n" + decals[0] + "\n#else\n" + decals[1] + "\n#endif\n")
