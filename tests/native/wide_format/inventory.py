"""Record literal/dynamic callers against the actual generated owning source inventory."""
import argparse
import hashlib
import json
from pathlib import Path
import re

p = argparse.ArgumentParser()
p.add_argument('--root', type=Path, required=True)
p.add_argument('--database', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
root = a.root.resolve()
selected = {Path(x['file']).resolve() for x in json.loads(a.database.read_text())}
names = ('_snwprintf', '_vsnwprintf', 'swprintf', 'vswprintf', 'SharedWVarArgs', 'BufWPrintf')
call = re.compile(r'\b(' + '|'.join(names) + r')\s*\(')
tokens = re.compile(r'//[^\n]*|/\*.*?\*/|L?"(?:\\.|[^"\\])*"|\x27(?:\\.|[^\x27\\])*\x27|[(),]', re.S)
records = []
hashes = {}
for source in sorted((root / 'GoldSrc-SDK').rglob('*')):
    if source.suffix not in ('.cpp', '.h') or 'external' in source.relative_to(root).parts:
        continue
    raw = source.read_bytes()
    text = raw.decode('cp1252')
    for match in call.finditer(text):
        depth = 1
        start = match.end()
        arguments = []
        end = None
        for token in tokens.finditer(text, start):
            if token.group() == '(':
                depth += 1
            elif token.group() == ')':
                depth -= 1
                if depth == 0:
                    arguments.append(text[start:token.start()].strip())
                    end = token.end()
                    break
            elif token.group() == ',' and depth == 1:
                arguments.append(text[start:token.start()].strip())
                start = token.end()
        assert end, source
        name = match.group(1)
        index = 0 if name == 'SharedWVarArgs' else 2
        if index >= len(arguments):
            continue
        fmt = arguments[index]
        literal = re.findall(r'L"((?:\\.|[^"\\])*)"', fmt)
        records.append({'path': str(source.relative_to(root)), 'line': text.count('\n', 0, match.start()) + 1,
            'function': name, 'selected_actual_TU': source.resolve() in selected,
            'kind': 'declaration_or_definition' if '...' in arguments else 'call',
            'arguments': arguments, 'format_expression': fmt, 'literal_parts': literal})
        hashes[str(source.relative_to(root))] = hashlib.sha256(raw).hexdigest()
for name in ('GoldSrc-SDK/common/legacy_text_encoding.h', 'GoldSrc-SDK/vgui2/src/LocalizedStringTable.cpp'):
    hashes[name] = hashlib.sha256((root / name).read_bytes()).hexdigest()
data = {'compiler_database': str(a.database), 'database_sha256': hashlib.sha256(a.database.read_bytes()).hexdigest(),
        'source_hashes': hashes, 'records': records}
a.output.write_text(json.dumps(data, indent=2) + '\n')
print('records', len(records), 'actual selected calls', sum(x['selected_actual_TU'] and x['kind'] == 'call' for x in records))
