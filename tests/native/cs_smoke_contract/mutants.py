#!/usr/bin/env python3
"""Reject concrete complete-body behavior changes against unchanged old reference."""
import argparse
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser()
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=False)
source = (ROOT / 'GoldSrc-SDK/cstrike/cl_dll/ev_hldm.cpp').read_text(encoding='cp1252')
start = source.index('void EV_HLDM_CreateSmoke(')
end = source.index('\n}\n#ifdef _MSC_VER', start) + 2
body = source[start:end]
changes = [
    ('direction-or', 'origin[0] != direction[0] && origin[1] != direction[1] && origin[2] != direction[2]',
     'origin[0] != direction[0] || origin[1] != direction[1] || origin[2] != direction[2]'),
    ('fallback', 'fallbackSmokeModel = 293', 'fallbackSmokeModel = 294'),
    ('full-type-zero', 'smokeType != 0 && g_SmokePuffQueue.Full()', 'g_SmokePuffQueue.Full()'),
    ('flag-overwrite', 'entity->flags |= FTENT_CLIENTCUSTOM', 'entity->flags = FTENT_CLIENTCUSTOM'),
    ('blue-channel', 'rendercolor.b = blue', 'rendercolor.b = red + 0 * blue'),
    ('scale-rng-bound', 'scale * 0.5', 'scale * 0.25'),
    ('base-velocity', '0.9 * baseVelocity[0]', '0.8 * baseVelocity[0]'),
    ('callback-swap', 'wind ? SmokeRise_Wind : SmokeRise', 'wind ? SmokeRise : SmokeRise_Wind'),
]
mutants = {}
for name, before, after in changes:
    assert body.count(before) == 1, (name, 'source drift')
    mutants[name] = body.replace(before, after)
cached = body.replace('\tint model;', '\tint model;\n    float cachedFast = 0;')
first = '\t\tif ( gEngfuncs.pfnGetCvarFloat( "fastsprites" ) > 1 )'
assert cached.count(first) == 2
cached = cached.replace(first, '        cachedFast = gEngfuncs.pfnGetCvarFloat( "fastsprites" );\n'
                        '        if ( cachedFast > 1 )', 1)
# Keep the first actual read, cache only subsequent queries.
position = cached.index('        if ( cachedFast > 1 )')
cached = cached[:position] + cached[position:].replace('gEngfuncs.pfnGetCvarFloat( "fastsprites" )', 'cachedFast')
mutants['cached-cvar'] = cached
rows = []
for name, changed in mutants.items():
    output = args.output / name
    output.mkdir()
    candidate = output / 'ev_hldm.cpp'
    candidate.write_bytes((source[:start] + changed + source[end:]).encode('cp1252'))
    build = ROOT / 'build' / ('cs-smoke-mutant-' + name)
    phases = []
    for label, command in [('configure', ['cmake', '-S', str(ROOT / 'tests/native/cs_smoke_contract'), '-B', str(build),
        '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_OSX_ARCHITECTURES=arm64',
        '-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0', '-DCSE_SMOKE_SOURCE=' + str(candidate)]),
        ('build', ['cmake', '--build', str(build), '--parallel', '2']),
        ('valid', [str(build / 'smoke_cases'), '--valid'])]:
        result = subprocess.run(command, capture_output=True, timeout=60)
        (output / (label + '.log')).write_bytes(result.stdout + result.stderr)
        phases.append({'phase': label, 'command': command, 'exit': result.returncode})
        if label != 'valid' and result.returncode:
            raise RuntimeError(name + ' must compile before its behavioral rejection')
    assert phases[-1]['exit'] != 0, name + ' mutant escaped'
    rows.append({'mutant': name, 'phases': phases})
    (args.output / 'commands.json').write_text(json.dumps(rows, indent=2))
    print(name, 'rejected by actual execution', flush=True)
