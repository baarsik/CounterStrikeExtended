"""Extract the production muzzle-flash body for controlled effect-boundary tests."""
from pathlib import Path
import re
import sys

source = Path(sys.argv[1]).read_text()
start = source.index("void R_MuzzleFlash( float* pos1, int type )")
masked = re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',
                lambda match: " " * len(match.group()), source, flags=re.DOTALL)
opening = masked.index("{", start)
depth = 1
end = opening + 1
while depth:
    depth += (masked[end] == "{") - (masked[end] == "}")
    end += 1
Path(sys.argv[2]).write_text("// Generated from production cl_tent.cpp; do not edit.\n"
                            + source[start:end] + "\n")
