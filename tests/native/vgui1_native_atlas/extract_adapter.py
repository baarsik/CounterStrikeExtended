"""Copy the real private embedded declaration/Draw method for a typed CPU receiver."""
import argparse
import hashlib
import json
import re
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
text = args.source.read_text()


def span(pattern):
    match = re.search(pattern, text, re.M)
    assert match
    first = text.index('{', match.end())
    depth = 1
    end = first + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[match.start():end]


record = span(r'^struct EmbeddedSurface') + ';'
method = span(r'^void Draw\(void\* context, const Quad& quad\) noexcept')
args.output.mkdir(exist_ok=True)
(args.output / 'embedded_adapter.inc').write_text('namespace cse_vgui1_native { namespace {\n'
    + record + '\n' + method + '\n} }\n')
(args.output / 'identity.json').write_text(json.dumps({'source': str(args.source),
    'sha256': hashlib.sha256(args.source.read_bytes()).hexdigest(),
    'declaration': record, 'method': method}, indent=2) + '\n')
