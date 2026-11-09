"""Extract unchanged constructor, methods and per-pixel expressions from the source."""
import hashlib
import json
from pathlib import Path
import sys

source = Path(sys.argv[1])
output = Path(sys.argv[2])
output.mkdir(parents=True, exist_ok=True)
raw = source.read_bytes()
text = raw.decode('cp1252')
start = text.index('\t\tconst int tenthsOfDegrees =')
end = text.index('\n\t}\n\n\tvirtual ~FontPlat()', start)
(output / 'actual_constructor.inc').write_text(text[start:end] + '\n')
start = text.index('\tvirtual void getCharRGBA(')
end = text.index('\n\tvirtual void drawSetTextFont(', start)
(output / 'actual_methods.inc').write_text(text[start:end] + '\n')
start = text.index('\t\t\t\tconst float blue =')
end = text.index('\n\t\t\t}', start)
(output / 'actual_pixel.inc').write_text(text[start:end] + '\n')
(output / 'source.json').write_text(json.dumps({'source': str(source),
    'sha256': hashlib.sha256(raw).hexdigest(),
    'adaptation': 'unchanged constructor/method/pixel bodies; reference owns checked GDI resources'}, indent=2) + '\n')
