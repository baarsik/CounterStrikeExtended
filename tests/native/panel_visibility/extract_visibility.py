"""Compile the owning methods unchanged against controlled panel/provider services."""
import hashlib
import json
from pathlib import Path
import sys


def function(source, signature):
    start = source.find(signature)
    if start < 0:
        return ""
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end] + "\n"


root = Path(sys.argv[1])
out = Path(sys.argv[2])
counter_strike = len(sys.argv) > 3 and sys.argv[3] == "cs"
html_source = "cstrike/vgui2/controls/HTML.cpp" if counter_strike else "vgui2/controls/HTML.cpp"
html_header = "cstrike/public/vgui_controls/HTML.h" if counter_strike else "vgui2/controls/HTML.h"
suffix = "_cs" if counter_strike else ""
methods = {
    "vgui2/controls/Panel.cpp": ["void Panel::SetVisible("],
    "vgui2/src/VPanel.cpp": ["static void NotifyEffectiveVisibilityChanged(",
                            "void VPanel::SetVisible(", "void VPanel::SetParent("],
    html_source: ["void HTML::OnCseEffectiveVisibilityChanged(", "void HTML::BrowserResize("],
}
hashes = {}
result = []
for relative, signatures in methods.items():
    path = root / "src" / relative
    data = path.read_bytes()
    source = data.decode("cp1252").replace("\r\n", "\n")
    hashes[relative] = {"sha256": hashlib.sha256(data).hexdigest(), "functions": {}}
    for signature in signatures:
        body = function(source, signature)
        hashes[relative]["functions"][signature] = hashlib.sha256(body.encode()).hexdigest()
        result.append(body)
header_bytes = (root / "src" / html_header).read_bytes()
header = header_bytes.decode("cp1252").replace("\r\n", "\n")
registration = "\n".join(line.strip() for line in header.splitlines()
                         if "MESSAGE_FUNC(OnCseEffectiveVisibilityChanged," in line)
hashes[html_header] = {"sha256": hashlib.sha256(header_bytes).hexdigest()}
(out / f"actual_visibility_handler{suffix}.inc").write_text(registration + "\n")
(out / f"actual_visibility{suffix}.inc").write_text("\n".join(result))
(out / f"producing-hashes{suffix}.json").write_text(json.dumps(hashes, indent=2) + "\n")
