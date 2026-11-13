"""Extract complete actual HUD scan/effect/Draw owners without changing their bodies."""
import hashlib
import json
from pathlib import Path
import sys

root = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2])
out.mkdir(parents=True, exist_ok=True)
owner = Path(sys.argv[3]) if len(sys.argv) > 3 else root / 'src/cl_dll/message.cpp'
source = owner.read_text()


def block(signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


signatures = [
    'float CHudMessage::FadeBlend(',
    'int\tCHudMessage::XPosition(',
    'int CHudMessage::YPosition(',
    'void CHudMessage::MessageScanNextChar(',
    'void CHudMessage::MessageScanStart(',
    'void CHudMessage::MessageDrawScan(',
    'int CHudMessage::Draw(',
]
cached_deadlines = 'float CHudMessage::MessageEndTime(' in source
if cached_deadlines:
    signatures.extend(('float CHudMessage::MessageEndTime(', 'void CHudMessage::ReleaseMessage('))
(out / 'message.inc').write_text('#define CSE_MESSAGE_CACHED_DEADLINES ' + str(int(cached_deadlines)) + '\n'
    + '\n\n'.join(block(s) for s in signatures) + '\n')
paths = [owner, root / 'src/cl_dll/legacy_message_text.h', root / 'src/common/legacy_text_encoding.h']
(out / 'source-manifest.json').write_text(json.dumps({
    str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths
}, indent=2) + '\n')
