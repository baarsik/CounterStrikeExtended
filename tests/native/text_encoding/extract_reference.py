"""Extract actual conversion function bodies, preserving the selected Win32 code."""
import argparse
import hashlib
import json
from pathlib import Path


def function(text, signature):
    begin = text.index(signature)
    brace = text.index('{', begin)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[begin:end]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    path = args.root / 'src/vgui2/src/LocalizedStringTable.cpp'
    raw = path.read_bytes()
    text = raw.decode('latin1')
    signatures = ['int Q_UCS2ToUTF8(', 'int CLocalizedStringTable::ConvertANSIToUnicode(',
                  'int CLocalizedStringTable::ConvertUnicodeToANSI(']
    bodies = [function(text, signature) for signature in signatures]
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / 'actual_functions.inc').write_bytes(('\n\n'.join(bodies) + '\n').encode('latin1'))
    (args.output / 'actual_source.json').write_text(json.dumps({'path': str(path),
        'sha256': hashlib.sha256(raw).hexdigest(),
        'windows_blocks': [hashlib.sha256(body[body.index('#ifdef _WIN32'):body.index('#else')].encode('latin1')).hexdigest()
                           for body in bodies]}, indent=2) + '\n')


if __name__ == '__main__':
    main()
