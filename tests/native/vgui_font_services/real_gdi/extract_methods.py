"""Extract unchanged VGUI1 constructor/raster/ABC bodies for a real memory-GDI oracle."""
import hashlib
import json
from pathlib import Path
import sys

source = Path(sys.argv[1])
out = Path(sys.argv[2])
out.mkdir(parents=True, exist_ok=True)
raw = source.read_bytes()
text = raw.decode('cp1252')
start = text.index('\t\tconst int tenthsOfDegrees =')
end = text.index('\n\t}\n\n\tvirtual ~FontPlat()', start)
(out / 'actual_constructor.inc').write_text(text[start:end] + '\n')
start = text.index('\tvirtual void getCharRGBA(')
end = text.index('\n\tvirtual void drawSetTextFont(', start)
(out / 'actual_methods.inc').write_text(text[start:end] + '\n')
(out / 'source.json').write_text(json.dumps({'source': str(source), 'sha256': hashlib.sha256(raw).hexdigest(),
    'adaptation': 'exact constructor body and raster/ABC/tall methods; fixture owns checked GDI resource lifetime'},
    indent=2) + '\n')
