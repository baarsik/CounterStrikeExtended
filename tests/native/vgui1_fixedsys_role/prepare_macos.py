"""Freeze/build only. Never run the real font probe, discovery or registration."""
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
parser.add_argument('--baseline', required=True, type=Path)
parser.add_argument('--build-base', required=True, type=Path)
parser.add_argument('--prefix', required=True, type=Path)
args = parser.parse_args()
root = args.root.resolve()
art = args.art.resolve()
art.mkdir(parents=True)
source = art / 'source'
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
baseline = json.loads((args.baseline / 'source-final-hashes.json').read_text())
provider = 'GoldSrc-SDK/utils/vgui/lib-src/vgui/posix/native/VGui1MacFontProvider.cpp'
files = list(baseline)
for name, expected in baseline.items():
    assert sha(args.baseline / 'source-final' / name) == expected, name
    assert name == provider or sha(root / name) == expected, name
native = 'GoldSrc-SDK/utils/vgui/lib-src/vgui/posix/native/'
files += [native + 'VGui1FixedSysFontFace.h', native + 'VGui1FixedSysFontFace.cpp']
files += [str(p.relative_to(root)) for p in sorted((root / 'tests/native/vgui1_fixedsys_role').iterdir())
          if p.is_file()]
for name in files:
    destination = source / name
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(root / name, destination)
hashes = {name: sha(source / name) for name in sorted(files)}
(art / 'source-hashes.json').write_text(json.dumps(hashes, indent=2) + '\n')
with tarfile.open(art / 'source.tar.gz', 'w:gz') as archive:
    for name in hashes:
        archive.add(source / name, arcname=name)


def record(command, log):
    with log.open('wb') as output:
        result = subprocess.run(command, stdout=output, stderr=subprocess.STDOUT, timeout=120)
    return {'command': [str(x) for x in command], 'exit': result.returncode, 'log': str(log)}


rows = []
future = []
selected = set()
for profile, configuration, instrument in [('release', 'Release', False), ('debug', 'Debug', False),
                                           ('sanitized', 'Debug', True)]:
    output = art / profile
    output.mkdir()
    build = args.build_base / profile
    assert not build.exists(), build
    commands = [('configure', ['cmake', '-S', str(source / 'tests/native/vgui1_fixedsys_role'),
        '-B', str(build), '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=' + configuration,
        '-DCMAKE_OSX_ARCHITECTURES=arm64', '-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0',
        '-DCSE_FREETYPE_PREFIX=' + str(args.prefix), '-DCMAKE_BUILD_RPATH=@loader_path',
        '-DCSE_PROVIDER_SANITIZERS=' + ('ON' if instrument else 'OFF')]),
        ('build', ['cmake', '--build', str(build), '--target', 'fixedsys_role_real_probe', '--parallel', '2'])]
    row = {'profile': profile, 'runtime-executed': False, 'phases': []}
    for phase, command in commands:
        row['phases'].append(record(command, output / (phase + '.log')))
        if row['phases'][-1]['exit']:
            break
    if len(row['phases']) == 2 and row['phases'][-1]['exit'] == 0:
        row['binary-hashes'] = {}
        images = [build / 'fixedsys_role_real_probe', build / 'baseline-provider/libcse_font_registration.dylib']
        for image in images:
            name = image.name
            shutil.copy2(image, output / name)
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
        future.append({'profile': profile, 'mode': 'fixedsys-role', 'authorized-or-executed': False,
            'timeout-seconds': 45, 'command': [str(output / 'fixedsys_role_real_probe'), '--fixedsys-role',
            '/System/Library/Fonts/Courier.ttc',
            str(art.parent / 'vgui1-fixedsys-role-real-execution-v1' / profile)]})
    rows.append(row)
    (art / 'build-cases.json').write_text(json.dumps(rows, indent=2) + '\n')
    print(profile, [x['exit'] for x in row['phases']], 'NOT RUN', flush=True)
(art / 'future-runtime-commands-NOT-RUN.json').write_text(json.dumps(future, indent=2) + '\n')
(art / 'selected-input-hashes.json').write_text(json.dumps(
    {str(p): sha(p) for p in sorted(selected)}, indent=2) + '\n')
(art / 'runtime-preconditions-NOT-RUN.json').write_text(json.dumps({
    'executed': False,
    'separate-authorization-required': True,
    'all-three-preconditions-before-first-query': [
        'all frozen source, image/service, selected dependency and compiler hashes unchanged',
        'exact Courier.ttc input SHA256 76cdad2ba4e8774223ed6184f530570261f9f7fb9beb5c68368e09234c0273ec',
        '45s external watchdog; fresh output directories; no automatic retries or extra modes',
        'exact one requested tuple/profile; no registration/UI/preferences/network/devices',
        'metrics/IDs/route/alpha facts/digests only; no font programs, tables or raster exports',
        'no owner-profile/bootstrap/factory/UI acceptance inferred from bare candidate measurement'],
    'outcomes': 'Ready vs ordinary Unavailable in facts.tsv; other error/timeout is execution failure',
    'resource-path': '/System/Library/Fonts/Courier.ttc',
    'resource-sha256': '76cdad2ba4e8774223ed6184f530570261f9f7fb9beb5c68368e09234c0273ec',
    'registration-calls': 0}, indent=2) + '\n')
assert all(sha(source / name) == digest for name, digest in hashes.items())
raise SystemExit(int(any(len(x['phases']) != 2 or x['phases'][-1]['exit'] for x in rows)))
