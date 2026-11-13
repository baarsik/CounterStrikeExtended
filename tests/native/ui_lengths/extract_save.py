"""Extract the exact save/conversion and existing UTF16 token reader bodies."""
import hashlib
import json
from pathlib import Path
import sys

root = Path(sys.argv[1]).resolve()
output = Path(sys.argv[2]).resolve()
blocks = []
receipt = []


def add(relative, signature):
    path = root / relative
    if relative.endswith('/LocalizedStringTable.cpp') and len(sys.argv) > 3:
        path = Path(sys.argv[3]).resolve()
    data = path.read_bytes()
    text = data.decode('latin1')
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    body = text[start:end]
    line = text.count('\n', 0, start) + 1
    blocks.append(f'#line {line} "{path.as_posix()}"\n{body}\n')
    receipt.append({'file': relative, 'source_sha256': hashlib.sha256(data).hexdigest(),
                    'line': line, 'body_sha256': hashlib.sha256(body.encode('latin1')).hexdigest()})


save_path = Path(sys.argv[3]) if len(sys.argv) > 3 else root / 'src/vgui2/src/LocalizedStringTable.cpp'
for signature in ('int CLocalizedStringTable::ConvertANSIToUnicode(', 'bool WriteLocalizationUTF16(',
                  'bool CLocalizedStringTable::SaveToFile('):
    if signature in save_path.read_bytes().decode('latin1'):
        add('src/vgui2/src/LocalizedStringTable.cpp', signature)
for signature in ('ucs2* AdvanceOverWhitespace(', 'ucs2* ReadUnicodeToken('):
    add('src/vgui2/src/public/UnicodeFileHelpers.cpp', signature)
output.mkdir(parents=True, exist_ok=True)
(output / 'localization_bodies.inc').write_text('\n'.join(blocks))
(output / 'extracted-localization.json').write_text(json.dumps(receipt, indent=2) + '\n')
