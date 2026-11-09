"""Inventory the newly pinned resource text corpus, without treating it as all allowed formats."""
import argparse
import hashlib
import json
from pathlib import Path
import re

p = argparse.ArgumentParser()
p.add_argument('--input', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
token = re.compile(r'//[^\n]*|"(?:\\.|[^"\\])*"|[{}]', re.S)
conversion = re.compile(r'%(?:%|[-+ #0]*(?:\d+|\*)?(?:\.(?:\d+|\*)?)?(?:I64|I32|hh|ll|[hlwjztIL])?[A-Za-z])')
files = []
formats = []
for source in sorted((a.input / 'resource-text').rglob('*')):
    if not source.is_file():
        continue
    raw = source.read_bytes()
    if raw.startswith((b'\xff\xfe', b'\xfe\xff')):
        text = raw.decode('utf-16')
        encoding = 'UTF16-BOM'
    else:
        try:
            text = raw.decode('utf-8-sig')
            encoding = 'UTF8'
        except UnicodeDecodeError:
            text = raw.decode('cp1252')
            encoding = 'cp1252-fallback-observed-bytes'
    records = [x for x in token.finditer(text) if not x.group().startswith('//')]
    index = 0
    while index + 1 < len(records):
        key = records[index]
        value = records[index + 1]
        if key.group().startswith('"') and value.group().startswith('"'):
            content = value.group()[1:-1]
            if '%' in content or 'voice_location' in key.group().lower():
                placeholders = re.findall(r'%s[0-9]+', content)
                rest = re.sub(r'%s[0-9]+', '', content)
                formats.append({'file': str(source.relative_to(a.input)),
                    'line': text.count('\n', 0, key.start()) + 1, 'key': key.group()[1:-1],
                    'value': content, 'Valve_placeholders': placeholders,
                    'remaining_printf_tokens': conversion.findall(rest)})
            index += 2
        else:
            index += 1
    files.append({'file': str(source.relative_to(a.input)), 'encoding': encoding,
                   'sha256': hashlib.sha256(raw).hexdigest()})
a.output.write_text(json.dumps({'scope': 'observed corpus only; resource values are not necessarily routed to printf',
    'files': files, 'format_records': formats}, ensure_ascii=True, indent=2) + '\n')
print('files', len(files), 'percent-bearing resource values', len(formats))
print('non-Valve printf tokens', sorted({t for x in formats for t in x['remaining_printf_tokens']}))
