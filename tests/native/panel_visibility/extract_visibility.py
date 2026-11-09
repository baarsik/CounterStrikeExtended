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
methods = {
    "vgui2/controls/Panel.cpp": ["void Panel::SetVisible("],
    "vgui2/src/VPanel.cpp": ["static void NotifyEffectiveVisibilityChanged(",
                            "void VPanel::SetVisible(", "void VPanel::SetParent("],
    "vgui2/controls/HTML.cpp": ["void HTML::OnCseEffectiveVisibilityChanged(", "void HTML::BrowserResize("],
}
hashes = {}
result = []
for relative, signatures in methods.items():
    path = root / "GoldSrc-SDK" / relative
    data = path.read_bytes()
    source = data.decode("cp1252").replace("\r\n", "\n")
    hashes[relative] = {"sha256": hashlib.sha256(data).hexdigest(), "functions": {}}
    for signature in signatures:
        body = function(source, signature)
        hashes[relative]["functions"][signature] = hashlib.sha256(body.encode()).hexdigest()
        result.append(body)
(out / "actual_visibility.inc").write_text("\n".join(result))
(out / "producing-hashes.json").write_text(json.dumps(hashes, indent=2) + "\n")
