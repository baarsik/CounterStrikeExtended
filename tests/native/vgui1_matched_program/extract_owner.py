"""Reuse the frozen checked GDI resource owner; its font bodies are extracted separately."""
import hashlib
import json
from pathlib import Path
import sys

path = Path(sys.argv[1])
output = Path(sys.argv[2])
raw = path.read_bytes()
text = raw.decode('utf8')
prefix = text[:text.index('void ProbeOrder(')]
(output / 'gdi_owner.inc').write_text(prefix + '\n}\n')
(output / 'owner-source.json').write_text(json.dumps({'source': str(path),
    'sha256': hashlib.sha256(raw).hexdigest(),
    'adaptation': 'Unchanged prefix through RealGdiFont; closes its anonymous namespace only'}, indent=2) + '\n')
