#!/usr/bin/env python3
"""Actual complete-body CPU fixture matrix, with exact trace/binary/source receipts."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser()
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--build-prefix', required=True)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=False)
records = []
inputs = [ROOT / 'src/cstrike/cl_dll/ev_hldm.cpp', ROOT / 'src/cstrike/cl_dll/hud.h',
          ROOT / 'src/cstrike/cl_dll/hud_redraw.cpp']
inputs += [p for p in Path(__file__).parent.rglob('*') if p.is_file() and '__pycache__' not in p.parts]
pins = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
for profile, config, sanitize in [('release', 'Release', False), ('debug', 'Debug', False), ('sanitized', 'Debug', True)]:
    output = args.output / profile
    output.mkdir()
    build = ROOT / 'build' / (args.build_prefix + '-' + profile)
    row = {'profile': profile, 'phases': []}
    commands = [('configure', ['cmake', '-S', str(ROOT / 'tests/native/cs_smoke_contract'), '-B', str(build),
                              '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=' + config, '-DCMAKE_OSX_ARCHITECTURES=arm64',
                              '-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0', '-DCSE_SMOKE_SANITIZERS=' + ('ON' if sanitize else 'OFF')]),
                ('build', ['cmake', '--build', str(build), '--parallel', '4']),
                ('ctest', ['ctest', '--test-dir', str(build), '--output-on-failure']),
                ('valid', [str(build / 'smoke_cases'), '--valid']),
                ('invalid', [str(build / 'smoke_cases'), '--invalid'])]
    for phase, command in commands:
        result = subprocess.run(command, cwd=ROOT, capture_output=True, timeout=120)
        (output / (phase + '.log')).write_bytes(result.stdout + result.stderr)
        row['phases'].append({'phase': phase, 'command': command, 'exit': result.returncode})
        if result.returncode:
            break
    records.append(row)
    (args.output / 'commands.json').write_text(json.dumps(records, indent=2))
    if any(p['exit'] for p in row['phases']):
        raise RuntimeError('Failed profile: ' + profile)
    image = build / 'smoke_cases'
    row['image_sha256'] = hashlib.sha256(image.read_bytes()).hexdigest()
    shutil.copy2(image, output / image.name)
    shutil.copy2(build / 'compile_commands.json', output / 'compile_commands.json')
    shutil.copytree(build / 'bodies', output / 'bodies')
    identity = subprocess.check_output(['/usr/bin/otool', '-hv', '-l', str(image)])
    (output / 'image-identity.txt').write_bytes(identity)
    assert all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == value for p, value in pins.items())
    print(profile, [p['exit'] for p in row['phases']], flush=True)
(args.output / 'commands.json').write_text(json.dumps(records, indent=2))
(args.output / 'source-pins-before-after.json').write_text(json.dumps(pins, indent=2))
