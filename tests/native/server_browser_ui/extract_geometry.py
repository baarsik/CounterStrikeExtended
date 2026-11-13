"""Generate an ISurface forwarding fixture and complete engine geometry bodies.

The injected geometry enters both actual CVGui and controls factories before the
real scheme loads. Font creation/measurement and panel traversal remain the
admitted real surface's implementation; no replacement font metrics are used.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path
from extract_owners import extract

parser = argparse.ArgumentParser()
parser.add_argument('--repo', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
interface = args.repo / 'src/engine/vgui/ISurface.h'
source = args.repo / 'src/engine/vgui2/BaseUISurface.cpp'
text = interface.read_text(errors='replace')
geometry = {'GetScreenSize', 'GetWorkspaceBounds', 'GetAbsoluteWindowBounds',
            'GetProportionalBase', 'GetHDProportionalBase'}
methods = re.findall(r'virtual\s+([^;]+?)\s*=\s*0\s*;', text)
assert len(methods) == 105, len(methods)
lines = []
for method in methods:
    match = re.fullmatch(r'(.+?)\s+(\w+)\s*\((.*?)\)', ' '.join(method.split()))
    assert match, method
    result, name, params = match.groups()
    names = [re.search(r'(\w+)\s*$', parameter).group(1) for parameter in params.split(',')] if params else []
    signature = f'{result} {name}({params}) override'
    if name == 'GetEmbeddedPanel':
        lines.append(signature + ' { return _embeddedPanel; }')
    elif name in {'DrawUnicodeChar', 'DrawUnicodeCharAdd'}:
        lines.append(signature + ' { if (observeText) { paintedText += ch; } real_->' + name + '(ch); }')
    elif name == 'DrawPrintText':
        lines.append(signature + ' { if (observeText) { paintedText.append(text, length); } real_->DrawPrintText(text, length); }')
    elif name == 'HasFocus':
        lines.append(signature + ' { return forceWindowFocus || real_->HasFocus(); }')
    elif name == 'GetPopupCount':
        lines.append(signature + ' { return forceCursorWithin ? 0 : real_->GetPopupCount(); }')
    elif name in {'IsCursorVisible', 'IsWithin'}:
        lines.append(signature + ' { return forceCursorWithin || real_->' + name + '(' + ', '.join(names) + '); }')
    else:
        lines.append(signature + (';' if name in geometry else
            ' { return real_->' + name + '(' + ', '.join(names) + '); }'))
(args.output / 'geometry_forward.inc').write_text('\n'.join(lines) + '\n')
receipts = []
bodies = []
for name in sorted(geometry):
    body, line = extract(source.read_text(errors='replace'), 'BaseUISurface::' + name, 0, 1)
    bodies.append(body)
    receipts.append(dict(source=str(source), symbol=name, line=line,
                         sourceSha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                         bodySha256=hashlib.sha256(body.encode()).hexdigest()))
(args.output / 'geometry_bodies.inc').write_text('\n\n'.join(bodies) + '\n')
(args.output / 'geometry-receipt.json').write_text(json.dumps(dict(
    interface=str(interface), interfaceSha256=hashlib.sha256(interface.read_bytes()).hexdigest(),
    methods=len(methods), bodies=receipts), indent=2) + '\n')
