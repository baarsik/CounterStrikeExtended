"""Defined wrong-initial-value mutant; never execute an uninitialized candidate."""
import argparse
import json
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--art', required=True, type=Path)
args = parser.parse_args()
args.art.mkdir(exist_ok=False)
repo = Path(__file__).resolve().parents[3]
source = repo / 'GoldSrc-SDK/utils/vgui/lib-src/vgui/DefaultCursor.cpp'
text = source.read_text()
assert text.count(': _hotspot{0, 0}') == 1
mutant = args.art / 'DefaultCursor-defined-one.cpp'
mutant.write_text(text.replace(': _hotspot{0, 0}', ': _hotspot{1, 0}'))
build = repo / 'build/vgui1-default-cursor-defined-mutant'
rows = []
for label, command, expected in [
    ('configure', ['cmake', '-S', str(Path(__file__).parent), '-B', str(build), '-G', 'Ninja',
                   '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_OSX_ARCHITECTURES=arm64',
                   '-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0', '-DCSE_CURSOR_SOURCE=' + str(mutant)], 0),
    ('build', ['cmake', '--build', str(build), '--parallel', '2'], 0),
    ('none-rejected', [str(build / 'default_cursor'), '--none'], 1),
    ('ordinary-unchanged', [str(build / 'default_cursor'), '--ordinary'], 0)]:
    with (args.art / (label + '.log')).open('wb') as log:
        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=90)
    rows.append({'phase': label, 'command': command, 'exit': result.returncode, 'expected': expected})
    (args.art / 'commands.json').write_text(json.dumps(rows, indent=2) + '\n')
    assert result.returncode == expected
