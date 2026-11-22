"""Expose unchanged production capture entry points with controlled GL/filesystem bindings."""
from pathlib import Path
import sys
import re


def function(source, signature):
    match = re.search(re.escape(signature) + r"[^;{}]*\{", source)
    if match is None:
        raise ValueError(f"Missing function: {signature}")
    start = match.start()
    opening = match.end() - 1
    depth = 1
    end = opening + 1
    while depth:
        if source[end] == "{":
            depth += 1
        elif source[end] == "}":
            depth -= 1
        end += 1
    return source[start:end] + "\n"


root = Path(sys.argv[1])
output = Path(sys.argv[2])
output.mkdir(parents=True, exist_ok=True)
for variant, screen, video in (
    ("hardware", "gl_screen.cpp", "gl_vidnt.cpp"),
    ("software", "screen.cpp", "vid_win.cpp"),
):
    bodies = f"namespace {variant} {{\n"
    source = (root / "src/engine" / screen).read_text(encoding="latin1")
    bodies += function(source, "void SCR_ScreenShot_f()")
    source = (root / "src/engine" / video).read_text(encoding="latin1")
    for signature in (
        "static void SwapSnapshotChannels(",
        "static void WriteSnapshotRows(",
        "static bool CaptureSnapshot(",
        "void VID_TakeSnapshot(",
        "void VID_TakeSnapshotRect(",
        "void VID_WriteBuffer(",
    ):
        if signature in source:
            bodies += function(source, signature)
    bodies += "}\n"
    (output / f"{variant}.inc").write_text(bodies, encoding="latin1")
source = (root / "src/engine/cl_main.cpp").read_text(encoding="latin1")
(output / "client.inc").write_text("namespace client {\nusing hardware::VID_TakeSnapshot;\n" +
    function(source, "void CL_TakeSnapshot_f()") + "}\n", encoding="latin1")

source = (root / "src/cstrike/dlls/client.cpp").read_text(encoding="latin1")
(output / "resources.inc").write_text(function(source, "void ClientPrecache()"), encoding="latin1")
