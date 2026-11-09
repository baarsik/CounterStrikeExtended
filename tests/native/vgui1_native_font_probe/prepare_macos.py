"""Freeze and build only; never execute the linked font probe or service."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import tarfile

parser = argparse.ArgumentParser()
parser.add_argument('--root', required=True, type=Path)
parser.add_argument('--art', required=True, type=Path)
parser.add_argument('--build-base', required=True, type=Path)
parser.add_argument('--prefix', required=True, type=Path)
args = parser.parse_args()
root = args.root.resolve()
art = args.art.resolve()
art.mkdir(parents=True)
source = art / 'source'
files = [p for p in (root / 'tests/native/vgui1_native_font_probe').iterdir() if p.is_file()]
for name in ['MacFontDiscovery.h', 'MacFontDiscovery.cpp', 'MacFontRegistration.h', 'MacFontRegistration.cpp']:
    files.append(root / 'GoldSrc-SDK/vgui2/vgui_surfacelib/native' / name)
for name in ['VGui1LegacyRaster.h', 'VGui1LegacyRaster.cpp']:
    files.append(root / 'GoldSrc-SDK/utils/vgui/lib-src/vgui/posix/native' / name)
for name in ['cmake/MacFontService.cmake', 'cmake/dependencies/vcpkg/vcpkg.json',
             'cmake/dependencies/vcpkg/triplets/cse-arm64-macos.cmake',
             'tests/native/vgui1_legacy_raster/digest.h', 'tests/native/vgui1_legacy_raster/make_owned_font.py']:
    files.append(root / name)
for path in files:
    destination = source / path.relative_to(root)
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(path, destination)
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
hashes = {str(p.relative_to(source)): sha(p) for p in sorted(source.rglob('*')) if p.is_file()}
(art / 'source-hashes.json').write_text(json.dumps(hashes, indent=2) + '\n')
with tarfile.open(art / 'source.tar.gz', 'w:gz') as archive:
    for name in hashes:
        archive.add(source / name, arcname=name)

def record(command, log):
    with log.open('wb') as output:
        result = subprocess.run(command, stdout=output, stderr=subprocess.STDOUT, timeout=90)
    return {'command': [str(x) for x in command], 'exit': result.returncode, 'log': str(log)}

# This runs only the sealed plain-data generator, never a font API/provider.
generated = record([str(Path(__import__('sys').executable)),
    str(source / 'tests/native/vgui1_legacy_raster/make_owned_font.py'), str(art / 'owned-inputs')],
    art / 'authored-generator.log')
assert generated['exit'] == 0
(art / 'authored-generator-command.json').write_text(json.dumps(generated, indent=2) + '\n')
rows = []
future = []
selected = set()
for profile, configuration, instrument in [('release', 'Release', False), ('debug', 'Debug', False),
                                            ('sanitizers', 'Debug', True)]:
    output = art / profile
    output.mkdir()
    build = args.build_base / profile
    assert not build.exists(), build
    commands = [('configure', ['cmake', '-S', str(source / 'tests/native/vgui1_native_font_probe'),
        '-B', str(build), '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=' + configuration,
        '-DCMAKE_OSX_ARCHITECTURES=arm64', '-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0',
        '-DCSE_FREETYPE_PREFIX=' + str(args.prefix),
        '-DCSE_PROBE_SANITIZERS=' + ('ON' if instrument else 'OFF')]),
        ('build', ['cmake', '--build', str(build), '--parallel', '2'])]
    row = {'profile': profile, 'runtime-executed': False, 'phases': []}
    for phase, command in commands:
        row['phases'].append(record(command, output / (phase + '.log')))
        if row['phases'][-1]['exit']:
            break
    if len(row['phases']) == 2 and row['phases'][-1]['exit'] == 0:
        row['binary-hashes'] = {}
        for name in ['vgui1_native_font_probe', 'libcse_font_registration.dylib']:
            shutil.copy2(build / name, output / name)
            row['binary-hashes'][name] = sha(output / name)
            record(['file', str(output / name)], output / (name + '-machine.log'))
            record(['otool', '-l', str(output / name)], output / (name + '-load-commands.log'))
            record(['otool', '-L', str(output / name)], output / (name + '-imports.log'))
        shutil.copy2(build / 'compile_commands.json', output / 'compile_commands.json')
        commands_text = subprocess.check_output(['ninja', '-C', str(build), '-t', 'commands'], text=True)
        (output / 'ninja-commands.log').write_text(commands_text)
        for line in commands_text.splitlines():
            for token in shlex.split(line):
                path = Path(token)
                if path.is_file() and path.suffix == '.a':
                    selected.add(path)
        for line in subprocess.check_output(['ninja', '-C', str(build), '-t', 'deps'], text=True).splitlines():
            path = Path(line.strip())
            if path.is_file() and source not in path.parents:
                selected.add(path)
        for mode, arguments in [('discover-needed', []),
                                ('owned-registration', [str(art / 'owned-inputs/owned-raster.ttf')])]:
            future.append({'profile': profile, 'mode': mode, 'authorized-or-executed': False,
                'timeout-seconds': 45, 'command': [str(build / 'vgui1_native_font_probe'), '--' + mode]
                + arguments + [str(output / (mode + '.tsv'))]})
    rows.append(row)
    (art / 'build-cases.json').write_text(json.dumps(rows, indent=2) + '\n')
    print(profile, [x['exit'] for x in row['phases']], 'NOT RUN', flush=True)
(art / 'future-runtime-commands-NOT-RUN.json').write_text(json.dumps(future, indent=2) + '\n')
(art / 'selected-input-hashes.json').write_text(json.dumps(
    {str(p): sha(p) for p in sorted(selected)}, indent=2) + '\n')
assert all(sha(source / name) == digest for name, digest in hashes.items())
raise SystemExit(int(any(len(x['phases']) != 2 or x['phases'][-1]['exit'] for x in rows)))
