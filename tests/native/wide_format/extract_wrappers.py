"""Extract actual retained shared wide wrapper bodies without production edits."""
import hashlib
import json
from pathlib import Path
import re
import sys

root = Path(sys.argv[1])
source = root / 'src/cstrike/game_shared/shared_util.cpp'
out = Path(sys.argv[2])
raw = source.read_bytes()
text = raw.decode('cp1252')
functions = []
for name in ('SharedWVarArgs', 'BufWPrintf'):
    match = re.search(r'wchar_t\s*\*\s*' + name + r'\([^\n]+\)\s*\{', text)
    assert match, name
    depth = 0
    tokens = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\x27(?:\\.|[^\x27\\])*\x27|[{}]', re.S)
    for token in tokens.finditer(text, match.end() - 1):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if depth == 0:
                functions.append(text[match.start():token.end()])
                break
out.write_bytes(('\n\n'.join(functions) + '\n').encode('utf-8'))
out.with_suffix('.json').write_text(json.dumps({'source': str(source), 'sha256': hashlib.sha256(raw).hexdigest(),
    'functions': ['SharedWVarArgs', 'BufWPrintf'], 'adaptation': 'none; typed formatter bindings selected in fixture'},
    indent=2) + '\n')
