"""Compile the exact PCX type/writer body, with only renderer dependencies supplied by the fixture."""
import argparse
import hashlib
import json
from pathlib import Path
import re


def function(text, signature):
    start = text.index(signature)
    cursor = text.index('{', start)
    depth = 1
    end = cursor + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return start, text[start:end]


def run(screen, zone, output):
    blocks = []
    receipt = []

    def add(path, start, body):
        text = path.read_text(encoding='latin1')
        line = text.count('\n', 0, start) + 1
        blocks.append(f'#line {line} "{path.as_posix()}"\n{body}\n')
        receipt.append({'file': str(path), 'line': line,
                        'body_sha256': hashlib.sha256(body.encode('latin1')).hexdigest()})

    text = screen.read_text(encoding='latin1')
    start = text.index('typedef struct\n{')
    end = text.index('viddef_t vid;', start)
    add(screen, start, text[start:end])
    text = zone.read_text(encoding='latin1')
    for name in ('HUNK_ALIGNMENT', 'HUNK_NAME_SIZE'):
        match = re.search(r'^#define ' + name + r'\s+[^\n]+', text, re.MULTILINE)
        add(zone, match.start(), match.group())
    start = text.index('typedef struct hunk_s')
    end = text.index('} hunk_t;', start) + len('} hunk_t;')
    add(zone, start, text[start:end])
    text = screen.read_text(encoding='latin1')
    add(screen, *function(text, 'void WritePCXfile('))
    output.mkdir(parents=True, exist_ok=True)
    (output / 'production_bodies.inc').write_text('\n'.join(blocks))
    (output / 'extracted-bodies.json').write_text(json.dumps(receipt, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('screen', type=Path)
    parser.add_argument('zone', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    run(args.screen.resolve(), args.zone.resolve(), args.output.resolve())
