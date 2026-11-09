"""Pin exact retained Win32 method bodies; only names become free functions."""
import hashlib
import json
from pathlib import Path
import sys

source = Path(sys.argv[1])
output = Path(sys.argv[2])
text = source.read_text()
names = ["getTimeMillis", "setClipboardText", "getClipboardTextCount", "getClipboardText"]
methods = []
receipt = {"source": str(source), "sha256": hashlib.sha256(source.read_bytes()).hexdigest(), "methods": {}}
for name in names:
    import re
    match = re.search(r"(?:long|void|int) App::" + name + r"\([^\n]*\)\n\{", text)
    assert match, name
    start = match.start()
    body = text.index("{", start)
    depth = 1
    end = body + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    method = text[start:end]
    receipt["methods"][name] = {"sha256": hashlib.sha256(method.encode()).hexdigest(), "line": text[:start].count("\n") + 1}
    methods.append(method.replace("App::", "", 1))
output.write_text("// Exact retained bodies, renamed as free functions, controlled WinAPI bindings.\n" + "\n\n".join(methods) + "\n")
output.with_suffix(".json").write_text(json.dumps(receipt, indent=2) + "\n")
