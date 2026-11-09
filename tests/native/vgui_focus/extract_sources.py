"""Compile unchanged production focus encoders, maps, and dispatchers with controlled UI bindings."""

from pathlib import Path
import sys


def function(source: str, signature: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
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
header = (root / "GoldSrc-SDK/vgui2/controls/Panel.h").read_text(encoding="latin1")
start = header.index("_MessageFuncCommon( OnRequestFocus")
start = header.rfind("\n", 0, start) + 1
previous = header.rfind("\n", 0, start - 1) + 1
if header[previous:start].startswith("#if"):
    start = previous
end = header.index("virtual void OnRequestFocus", start)
end = header.index(";", end) + 1
(output / "focus_map.inc").write_text(header[start:end], encoding="latin1")
for variant, relative in (
    ("shared", "GoldSrc-SDK/vgui2/controls/Panel.cpp"),
    ("cs", "GoldSrc-SDK/cstrike/vgui2/controls/Panel.cpp"),
):
    source = (root / relative).read_text(encoding="latin1")
    bodies = function(source, "void PreparePanelMessageMap(")
    bodies += "\nnamespace vgui2 {\n"
    for signature in (
        "void Panel::OnRequestFocus(",
        "void Panel::OnMessage(",
        "void Panel::PreparePanelMap(",
        "void Panel::OnOldMessage(",
    ):
        bodies += function(source, signature) + "\n"
    bodies += "}\n"
    (output / f"{variant}_bodies.inc").write_text(bodies, encoding="latin1")
