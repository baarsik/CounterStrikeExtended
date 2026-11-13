#!/usr/bin/env python3
"""Extract intact production studio attribute helper for the CPU-only fixture.

Usage: extract_engine.py REPOSITORY OUTPUT_INCLUDE
The fixture supplies engine globals and the light-math dependency, never a copy
of the attribute integration under test.
"""
from pathlib import Path
import re
import sys


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    root = Path(sys.argv[1]).resolve()
    source = root / "src/engine/r_studio.cpp"
    text = source.read_text()
    signature = "bool R_StudioDrawIndexedMesh("
    if text.count(signature) != 1:
        raise SystemExit("Expected exactly one production indexed-mesh helper")
    start = text.index(signature)
    # Mask literals/comments before counting so formatting and explanatory braces
    # cannot accidentally make the fixture extract a partial function or namespace.
    masked = re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                    lambda match: " " * len(match.group()), text, flags=re.DOTALL)
    opening = masked.index("{", start)
    depth = 1
    end = opening + 1
    while depth and end < len(masked):
        depth += (masked[end] == "{") - (masked[end] == "}")
        end += 1
    if depth:
        raise SystemExit("Unterminated production indexed-mesh helper")
    helper = text[start:end]
    local = (root / "src/engine/r_local.h").read_text()
    auxiliary = re.search(r"typedef struct\s*\{[^{}]*\}\s*auxvert_t;", local)
    if auxiliary is None:
        raise SystemExit("Could not extract the exact production auxvert_t")
    output = Path(sys.argv[2])
    output.parent.mkdir(parents=True, exist_ok=True)
    # auxvert_t is included separately so the globals precede the intact helper.
    output.write_text("// Generated from production; do not edit.\n"
                      "#if defined(CSE_STUDIO_EXTRACT_AUXVERT)\n"
                      + auxiliary.group() + "\n#else\n"
                      + f'#line {text.count(chr(10), 0, start) + 1} "{source.as_posix()}"\n'
                      + helper + "\n#endif\n")


if __name__ == "__main__":
    main()
