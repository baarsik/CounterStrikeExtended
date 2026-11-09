"""Owned real-loader fixtures only; no AppCold publication, fonts, provider or UI."""
import argparse
import hashlib
import json
import os
import shlex
import shutil
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--art', required=True, type=Path)
args = parser.parse_args()
repo = Path(__file__).resolve().parents[3]
args.art.mkdir(exist_ok=False)
assert not any(key in os.environ for key in ['ASAN_OPTIONS', 'UBSAN_OPTIONS', 'TSAN_OPTIONS'])
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
sources = [repo / 'GoldSrc-SDK/launcher/native' / name for name in
           ['VGui1HostImageOwner.h', 'VGui1HostImageOwner.cpp']]
sources += [repo / 'GoldSrc-SDK/public/native_module.h']
sources += sorted(path for path in Path(__file__).parent.iterdir() if path.is_file())
pins = {str(path): sha(path) for path in sources}
(args.art / 'pins-before.json').write_text(json.dumps(pins, indent=2) + '\n')
for path in sources:
    destination = args.art / 'source' / path.relative_to(repo)
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(path, destination)
rows = []


def execute(label, command, cwd=repo, expected=0):
    command = [str(value) for value in command]
    with (args.art / (label + '.log')).open('wb') as output:
        result = subprocess.run(command, cwd=cwd, stdout=output, stderr=subprocess.STDOUT, timeout=60)
    rows.append({'label': label, 'command': command, 'cwd': str(cwd),
                 'exit': result.returncode, 'expected': expected, 'watchdogSeconds': 60})
    (args.art / 'commands.json').write_text(json.dumps(rows, indent=2) + '\n')
    assert result.returncode == expected, (label, result.returncode)


for profile, config, sanitized in [('release', 'Release', False), ('debug', 'Debug', False),
                                    ('sanitized', 'Debug', True), ('path space Ω', 'Release', False)]:
    label = profile.replace(' ', '-')
    build = repo / ('build/vgui1-host-image-owner-final-' + profile)
    execute(label + '-configure', ['cmake', '-S', Path(__file__).parent, '-B', build, '-G', 'Ninja',
        '-DCMAKE_BUILD_TYPE=' + config, '-DCMAKE_OSX_ARCHITECTURES=arm64',
        '-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0', '-DCSE_OWNER_SOURCE_ROOT=' + str(args.art / 'source'),
        '-DCSE_OWNER_SANITIZERS=' + ('ON' if sanitized else 'OFF')])
    execute(label + '-build', ['cmake', '--build', build, '--parallel', '2'])
    execute(label + '-tests', ['ctest', '--test-dir', build, '--output-on-failure', '--timeout', '45'])
    shutil.copy2(build / 'compile_commands.json', args.art / (label + '-compile-commands.json'))
    shutil.copy2(build / 'Testing/Temporary/LastTest.log', args.art / (label + '-actual-cases.log'))
    for name in ['host_native', 'host_injected', 'libroot_fixture.dylib', 'libforeign_fixture.dylib',
                 'libproxy_fixture.dylib', 'libmisplaced_owner.dylib']:
        image = build / name
        shutil.copy2(image, args.art / (label + '-' + name))
        execute(label + '-' + name + '-machine', ['file', image])
        execute(label + '-' + name + '-build-version', ['xcrun', 'vtool', '-show-build', image])
        execute(label + '-' + name + '-imports', ['otool', '-L', image])
        assert 'arm64' in (args.art / (label + '-' + name + '-machine.log')).read_text()
        assert 'minos 14.0' in (args.art / (label + '-' + name + '-build-version.log')).read_text()
    execute(label + '-native-imports', ['nm', '-u', build / 'libowner_native.a'])

# NEW source policy is derived from a real owning launcher recipe, not selected
# production membership. Exact recipe keeps its inherited Sync/RTTI settings.
art_root = args.art.parent.parent
for profile in ['release', 'debug']:
    database_path = art_root / ('macos-runtime-policy-' + profile) / 'compile_commands.json'
    database = json.loads(database_path.read_text())
    entry, = [value for value in database if value['file'].endswith('/launcher/launcher.cpp')]
    command = shlex.split(entry['command'])
    command[command.index('-c') + 1] = str(args.art / 'source/GoldSrc-SDK/launcher/native/VGui1HostImageOwner.cpp')
    command[command.index('-o') + 1] = str(args.art / (profile + '-derived-launcher.o'))
    execute(profile + '-derived-launcher-policy', command, cwd=entry['directory'])
    (args.art / (profile + '-database-identity.json')).write_text(json.dumps({
        'path': str(database_path), 'sha256': sha(database_path), 'source': entry['file'],
        'scope': 'Derived NEW-source compile only; no manifest or production target build.'}, indent=2) + '\n')

for standard in ['c++14', 'c++17']:
    execute('typed-boundary-' + standard, ['xcrun', 'clang++', '-std=' + standard, '-arch', 'arm64',
        '-mmacosx-version-min=14.0', '-fno-exceptions', '-fno-rtti', '-Wall', '-Wextra', '-Werror',
        '-I' + str(args.art / 'source/GoldSrc-SDK/launcher/native'), '-S', '-emit-llvm',
        Path(__file__).parent / 'boundary_probe.cpp', '-o', args.art / ('boundary-' + standard + '.ll')])
for label, command in [('compiler', ['xcrun', 'clang++', '--version']), ('os', ['sw_vers']),
                       ('machine', ['uname', '-a'])]:
    execute(label, command)
final = {str(path): sha(path) for path in sources}
(args.art / 'pins-after.json').write_text(json.dumps(final, indent=2) + '\n')
assert final == pins, 'source changed during final qualification'
