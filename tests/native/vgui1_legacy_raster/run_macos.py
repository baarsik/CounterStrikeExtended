"""Freeze the approved source before native data-only producers."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--root', type=Path, required=True)
parser.add_argument('--packet', type=Path, required=True)
parser.add_argument('--build-base', type=Path, required=True)
parser.add_argument('--prefix', type=Path, required=True)
parser.add_argument('--reference', type=Path, required=True)
args = parser.parse_args()
root = args.root.resolve()
packet = args.packet.resolve()
packet.mkdir(parents=True)
source = packet / 'source'
files = list((root / 'tests/native/vgui1_legacy_raster').glob('*'))
files += list((root / 'src/utils/vgui/lib-src/vgui/posix/native').glob('VGui1LegacyRaster.*'))
files += [root / 'src/utils/vgui/lib-src/vgui/win32/Font.cpp',
          root / 'src/utils/vgui/lib-src/vgui/common/VGui1TextEncoding.h']
for file in files:
    if file.is_file():
        output = source / file.relative_to(root)
        output.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(file, output)
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
hashes = {str(path.relative_to(source)): sha(path) for path in sorted(source.rglob('*')) if path.is_file()}
(packet / 'source-hashes.json').write_text(json.dumps(hashes, indent=2) + '\n')
environment = os.environ.copy()
environment['ASAN_OPTIONS'] = 'detect_leaks=0:halt_on_error=1'
environment['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
rows = []
for profile, configuration, sanitizer in [('release', 'Release', False), ('debug', 'Debug', False),
                                           ('sanitizers', 'Debug', True)]:
    output = packet / profile
    output.mkdir()
    build = args.build_base / profile
    assert not build.exists()
    commands = [('configure', ['cmake', '-S', str(source / 'tests/native/vgui1_legacy_raster'), '-B', str(build),
        '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=' + configuration, '-DCMAKE_OSX_ARCHITECTURES=arm64',
        '-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0', '-DCSE_FREETYPE_PREFIX=' + str(args.prefix),
        '-DCSE_GDI_REFERENCE_DIRECTORY=' + str(args.reference),
        '-DCSE_RASTER_SANITIZERS=' + ('ON' if sanitizer else 'OFF')]),
        ('build', ['cmake', '--build', str(build), '--parallel', '2']),
        ('tests', ['ctest', '--test-dir', str(build), '--output-on-failure']),
        ('rgb-digest', [str(build / 'raster_pixel_tests'), 'rgb']),
        ('coverage-comparison', [str(Path(os.sys.executable)), str(source / 'tests/native/vgui1_legacy_raster/compare_candidate.py'),
            '--reference', str(args.reference), '--candidate', str(build / 'candidate-output'),
            '--output', str(output / 'comparison.json')])]
    row = {'profile': profile, 'phases': []}
    for name, command in commands:
        with (output / (name + '.log')).open('wb') as log:
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, env=environment, timeout=90)
        row['phases'].append({'phase': name, 'command': command, 'exit': result.returncode})
        if result.returncode and name != 'coverage-comparison':
            break
    if len(row['phases']) == 5:
        comparison = json.loads((output / 'comparison.json').read_text())
        assert row['phases'][-1]['exit'] == 1 and all(record['metrics_equal']
            and not record['mapping_mismatch_bytes'] and not record['abc_mismatches']
            and record['rgba_differing_bytes'] for record in comparison)
        for name in ('raster_pixel_tests', 'raster_candidate_tests', 'raster_replay_tests',
                     'liblegacy_raster.a', 'compile_commands.json'):
            shutil.copy2(build / name, output / name)
        shutil.copytree(build / 'candidate-output', output / 'candidate-output')
        shutil.copytree(build / 'owned-inputs', output / 'owned-inputs')
        row['binary_sha256'] = {name: sha(output / name) for name in ('raster_pixel_tests',
            'raster_candidate_tests', 'raster_replay_tests', 'liblegacy_raster.a')}
        for label, command in [('machine', ['file', str(output / 'raster_candidate_tests')]),
            ('load-commands', ['otool', '-l', str(output / 'raster_candidate_tests')]),
            ('imports', ['otool', '-L', str(output / 'raster_candidate_tests')])]:
            with (output / (label + '.log')).open('wb') as log:
                subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
    rows.append(row)
    (packet / 'cases.json').write_text(json.dumps(rows, indent=2) + '\n')
    print(profile, [(phase['phase'], phase['exit']) for phase in row['phases']], flush=True)
assert all(sha(source / name) == digest for name, digest in hashes.items())
raise SystemExit(int(any(len(row['phases']) != 5 or any(phase['exit'] for phase in row['phases'][:-1]) for row in rows)))
