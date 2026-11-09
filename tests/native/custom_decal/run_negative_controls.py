"""Build isolated imported-body mutations; never edit production or retained reference files."""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--repo', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=False)
records = []


def run(name, argv, expected):
    with (args.output / (name + '.log')).open('wb') as log:
        result = subprocess.run(list(map(str, argv)), stdout=log, stderr=subprocess.STDOUT, timeout=120)
    records.append(dict(name=name, argv=list(map(str, argv)), exit=result.returncode, expected=expected))
    (args.output / 'commands.json').write_text(json.dumps(records, indent=2) + '\n')
    assert result.returncode == expected, (name, result.returncode)


for case, target, filename, old, new in [
    ('cache-failure-published', 'custom_pipeline_hw', 'custom_functions.inc',
     '        Cache_Free(&pic->cache);\n        return FALSE;', '        return FALSE;'),
    ('nonheap-hook-admitted', 'custom_loader_tests', 'loader.inc',
     'if (requireHeap && usehunk != LOADFILE_HEAP)', 'if (false && requireHeap && usehunk != LOADFILE_HEAP)'),
    ('native-prefix-overlap', 'custom_builder_hw', 'draw.h',
     '(static_cast<int>(sizeof(texture_t) - sizeof(miptex_t)))', '32')]:
    build = args.output / case
    run(case + '-configure', ['cmake', '-S', args.repo / 'tests/native/custom_decal', '-B', build,
        '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Debug'], 0)
    path = build / 'imported' / filename
    if filename == 'draw.h':
        path.write_bytes((args.repo / 'GoldSrc-SDK/engine/draw.h').read_bytes())
    original = path.read_text()
    assert old in original, (case, old)
    changed = original.replace(old, new)
    path.write_text(changed)
    records.append(dict(mutation=case, path=str(path), original_sha256=hashlib.sha256(original.encode()).hexdigest(),
                        mutated_sha256=hashlib.sha256(changed.encode()).hexdigest()))
    run(case + '-build', ['cmake', '--build', build, '--target', target, '--parallel', '2'], 0)
    argv = [build / target]
    if target == 'custom_pipeline_hw':
        argv.append(build / 'authored/canonical.wad')
    run(case + '-must-fail', argv, 17)
print('Three isolated behavioral mutations rejected with exit17')
