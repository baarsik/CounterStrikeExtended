"""Extract the complete actual owner without rewriting its function or global."""
import argparse
import hashlib
import json
from pathlib import Path
import re

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--out', type=Path, required=True)
args = parser.parse_args()
source = args.source.resolve()
raw = source.read_bytes()
start = raw.index(b'void Con_DrawNotify()')
opening = raw.index(b'{', start)
depth = 1
end = opening + 1
while depth:
    depth += (raw[end] == ord('{')) - (raw[end] == ord('}'))
    end += 1
body = raw[start:end]
declaration, = re.findall(rb'^char g_szNotifyAreaString\[\d+\];', raw, re.MULTILINE)
args.out.mkdir(parents=True, exist_ok=True)
for name, value in [('notify.inc', body), ('notify_global.inc', declaration)]:
    (args.out / name).write_bytes(value + b'\n')
pins = dict(source=str(source), sourceSha256=hashlib.sha256(raw).hexdigest(),
            functionSha256=hashlib.sha256(body).hexdigest(),
            declarationSha256=hashlib.sha256(declaration).hexdigest(),
            extraction='complete function, original bytes; trailing LF added to include files',
            includes={name: hashlib.sha256((args.out / name).read_bytes()).hexdigest()
                      for name in ['notify.inc', 'notify_global.inc']})
(args.out / 'source-pins.json').write_text(json.dumps(pins, indent=2) + '\n')
