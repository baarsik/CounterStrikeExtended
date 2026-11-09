#!/usr/bin/env python3
"""Extract complete actual bodies and retained MSVC pragma framing, with byte hashes."""
import argparse
import hashlib
import json
from pathlib import Path
import re


def function(text, name):
    match = re.search(r'^(?:void |Queue::)' + re.escape(name) + r'\s*\(', text, re.M)
    if not match:
        raise RuntimeError('Missing actual function: ' + name)
    opening = text.index('{', match.start())
    depth = 1
    quote = None
    escaped = False
    for index in range(opening + 1, len(text)):
        char = text[index]
        if quote:
            if escaped:
                escaped = False
            elif char == '\\':
                escaped = True
            elif char == quote:
                quote = None
        elif char in ('"', "'"):
            quote = char
        elif char == '{':
            depth += 1
        elif char == '}':
            depth -= 1
            if depth == 0:
                return text[match.start():index + 1] + '\n'
    raise RuntimeError('Truncated function: ' + name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--reference', type=Path, required=True)
    parser.add_argument('--queue-source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    options = parser.parse_args()
    options.output.mkdir(parents=True, exist_ok=True)
    source = options.source.read_bytes()
    original = options.reference.read_bytes()
    native = source.decode('cp1252')
    old = original.decode('cp1252')
    includes = old[:old.index('#define SND_CHANGE_PITCH')]
    (options.output / 'includes.inc').write_bytes(includes.encode('cp1252'))
    functions = ['SmokeRise_Wind_Expand', 'HugWalls', 'SmokeRise_Wind', 'SmokeRise', 'EV_HLDM_CreateSmoke']
    records = []
    for label, text in [('reference', old), ('candidate', native)]:
        blocks = []
        for name in functions:
            body = function(text, name)
            if name != 'EV_HLDM_CreateSmoke':
                assert body == function(old, name), 'Unreviewed callback source change'
            else:
                # EXACT existing source-selected policy; no new warning disables.
                start = text.index(body)
                before = text[text.rfind('#ifdef _MSC_VER', 0, start):start]
                end = start + len(body)
                after = text[end:text.index('#endif', end) + len('#endif')]
                assert before == '#ifdef _MSC_VER\n#pragma warning( push )\n#pragma warning( disable : 4701 )\n#endif\n'
                assert after == '#ifdef _MSC_VER\n#pragma warning( pop )\n#endif'
                body = before + body + after + '\n'
            blocks.append(body)
            records.append({'role': label, 'function': name,
                            'sha256': hashlib.sha256(body.encode('cp1252')).hexdigest()})
        (options.output / (label + '.inc')).write_bytes('\n'.join(blocks).encode('cp1252'))
    queue = function(options.queue_source.read_text(encoding='cp1252'), '~Queue')
    (options.output / 'queue-destructor.inc').write_bytes(queue.encode('cp1252'))
    (options.output / 'bodies.json').write_text(json.dumps({'functions': records,
        'source_sha256': hashlib.sha256(source).hexdigest(), 'reference_sha256': hashlib.sha256(original).hexdigest(),
        'queue_source_sha256': hashlib.sha256(options.queue_source.read_bytes()).hexdigest(),
        'hud_time_seam': 'only actual declared float value; no CHud object/layout substitution'}, indent=2))


if __name__ == '__main__':
    main()
