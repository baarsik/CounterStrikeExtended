"""Extract whole actual decimal consumer methods; adapt only their fixture class qualifiers."""
import hashlib
import json
from pathlib import Path
import re
import sys

root = Path(sys.argv[1])
out = Path(sys.argv[2])
items = [('engine/utlbuffer.cpp', 'EngineBuffer'), ('vgui2/src/public/UtlBuffer.cpp', 'VguiBuffer'),
         ('cstrike/public/UtlBuffer.cpp', 'CstrikeBuffer')]
functions = []
receipts = []
for relative, name in items:
    source = root / 'src' / relative
    data = source.read_bytes()
    text = data.decode('cp1252')
    match = re.search(r'int CUtlBuffer::VaScanf\([^\n]+\)\s*\{', text)
    assert match, source
    depth = 0
    tokens = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\x27(?:\\.|[^\x27\\])*\x27|[{}]', re.S)
    end = None
    for token in tokens.finditer(text, match.end() - 1):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                end = token.end()
                break
    assert end
    body = text[match.start():end]
    functions.append(body.replace('CUtlBuffer::VaScanf', name + '::VaScanf') + '\n')
    receipts.append({'file': str(source), 'source_sha256': hashlib.sha256(data).hexdigest(),
                     'extracted_text_sha256': hashlib.sha256(body.encode()).hexdigest(),
                     'only_text_adaptation': 'CUtlBuffer::VaScanf -> ' + name + '::VaScanf'})
out.write_bytes('\n'.join(functions).encode('utf-8'))
out.with_suffix('.json').write_text(json.dumps(receipts, indent=2) + '\n')
