"""Extract pinned Cocoa dispatch unchanged; no backend/upstream edits."""
from pathlib import Path
import hashlib
import json
import sys

source = Path(sys.argv[1]) / 'src/video/cocoa/SDL_cocoamouse.m'
out = Path(sys.argv[2])
data = source.read_bytes()
assert hashlib.sha256(data).hexdigest() == 'ae75c94ddb34401eb3f070de0040c9c6d7f591ca4a6341f1d8a931f913282b4e'
text = data.decode()
begin = text.index('void Cocoa_HandleMouseEvent(')
end = text.index('\nvoid Cocoa_HandleMouseWheel', begin)
body = text[begin:end]
(out / 'cocoa_delta.inc').write_text(body)
(out / 'cocoa_delta.json').write_text(json.dumps(dict(source=str(source), source_sha256=hashlib.sha256(data).hexdigest(),
    body_sha256=hashlib.sha256(body.encode()).hexdigest(),
    seams='NSEvent/NSWindow and SDL mouse state/SendMouseMotion are OS/SDL boundaries; dispatch body unchanged'), indent=2) + '\n')
