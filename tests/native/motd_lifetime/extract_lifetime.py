"""Extract the actual close, navigation, and owner cleanup methods without edits."""

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
controller = (root / "src/engine/html/native/browser_controller.cpp").read_text(encoding="latin1")
(output / "detach.inc").write_text(
    function(controller, "void Detach()") + function(controller, "IHTMLResponses_HL1* CloseResponses()"))
owner = (root / "src/cstrike/vgui2/game_controls/clientmotd.cpp").read_text(encoding="latin1")
owner_methods = ""
for signature in ("CClientMOTD::~CClientMOTD()", "bool CClientMOTD::IsURL(",
                  "void CClientMOTD::Activate( const char*"):
    owner_methods += function(owner, signature)
(output / "owner.inc").write_text(owner_methods, encoding="latin1")
for variant, prefix in (("shared", "src/vgui2/controls"),
                        ("cs", "src/cstrike/vgui2/controls")):
    html = (root / prefix / "HTML.cpp").read_text(encoding="latin1")
    panel = (root / prefix / "Panel.cpp").read_text(encoding="latin1")
    methods = ""
    if variant == "shared":
        methods += function(html, "static void AssignString(")
    methods += "namespace vgui2 {\n"
    if variant == "shared":
        methods += function(html, "static void StorePendingNavigation(")
        methods += function(html, "static void SubmitURL(")
    for signature in ("HTML::~HTML()", "void HTML::BrowserClose()", "void HTML::OpenURL("):
        methods += function(html, signature)
    for signature in ("Panel::~Panel()", "void Panel::MarkForDeletion()"):
        methods += function(panel, signature)
    methods += "}\n"
    (output / f"{variant}_lifetime.inc").write_text(methods, encoding="latin1")
