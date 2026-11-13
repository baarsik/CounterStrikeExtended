"""Scoped actual-owner CPU scenes; no UI or production provider selection."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import signal
import subprocess
import tarfile
import time

parser = argparse.ArgumentParser()
parser.add_argument('--root', required=True, type=Path)
parser.add_argument('--art', required=True, type=Path)
parser.add_argument('--build-base', required=True, type=Path)
parser.add_argument('--prefix', required=True, type=Path)
parser.add_argument('--previous-prepared', required=True, type=Path)
parser.add_argument('--game-marlett', required=True, type=Path)
args = parser.parse_args()
root = args.root.resolve()
art = args.art.resolve()
art.mkdir(parents=True, exist_ok=True)
source = art / 'source'
assert not source.exists()
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
previous = json.loads((args.previous_prepared / 'source-hashes.json').read_text())
root_deltas = {
    'src/utils/vgui/lib-src/vgui/posix/VGui1FontServices.cpp':
        '5b4d0a4bbe367b2d0889f74e944cf9142e81abb42604b57feb5d15f3f1db0609',
    'tests/native/vgui1_native_provider/CMakeLists.txt':
        'fecc6e12138a2b2914e742bdf73713683f4d381779cee6c5c6cd8c0382508066',
    'tests/native/vgui1_fixedsys_role/CMakeLists.txt':
        '232204d45598eae22538e6692ebc131410bc5adba58bdf3a92544064824e4c36'}
for name, digest in previous.items():
    assert sha(root / name) == root_deltas.get(name, digest), name
files = list(previous)
files += ['src/engine/VGUI_EngineSurfaceWrap.cpp',
          'src/utils/vgui/lib-src/vgui/TextEntry.cpp']
files += [str(p.relative_to(root)) for p in sorted((root / 'tests/native/vgui1_fixedsys_readability').iterdir())
          if p.is_file()]
for name in files:
    target = source / name
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(root / name, target)
source_hashes = {name: sha(source / name) for name in sorted(files)}
(art / 'source-hashes.json').write_text(json.dumps(source_hashes, indent=2) + '\n')
(art / 'root-post-freeze-deltas.json').write_text(json.dumps(root_deltas, indent=2) + '\n')
with tarfile.open(art / 'source.tar.gz', 'w:gz') as archive:
    for name in source_hashes:
        archive.add(source / name, arcname=name)


def record(command, log):
    with log.open('wb') as output:
        result = subprocess.run(command, stdout=output, stderr=subprocess.STDOUT, timeout=120)
    return {'command': [str(x) for x in command], 'exit': result.returncode, 'log': str(log)}


rows = []
selected = set()
for profile, configuration, instrument in [('release', 'Release', False), ('debug', 'Debug', False),
                                           ('sanitized', 'Debug', True)]:
    output = art / profile
    output.mkdir()
    build = args.build_base / profile
    assert not build.exists()
    commands = [('configure', ['cmake', '-S', str(source / 'tests/native/vgui1_fixedsys_readability'),
        '-B', str(build), '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=' + configuration,
        '-DCMAKE_OSX_ARCHITECTURES=arm64', '-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0',
        '-DCSE_FREETYPE_PREFIX=' + str(args.prefix), '-DCMAKE_BUILD_RPATH=@loader_path',
        '-DCSE_PROVIDER_SANITIZERS=' + ('ON' if instrument else 'OFF')]),
        ('build', ['cmake', '--build', str(build), '--target', 'fixedsys_owner_readability', '--parallel', '3'])]
    row = {'profile': profile, 'phases': [], 'runtime-executed': False}
    for phase, command in commands:
        row['phases'].append(record(command, output / (phase + '.log')))
        assert row['phases'][-1]['exit'] == 0, (profile, phase)
    row['binary-hashes'] = {}
    for image in [build / 'fixedsys_owner_readability', build / 'baseline-provider/libcse_font_registration.dylib']:
        shutil.copy2(image, output / image.name)
        row['binary-hashes'][image.name] = sha(output / image.name)
        for suffix, command in [('machine', ['file', str(output / image.name)]),
                               ('load-commands', ['otool', '-l', str(output / image.name)]),
                               ('imports', ['otool', '-L', str(output / image.name)])]:
            record(command, output / (image.name + '-' + suffix + '.log'))
    shutil.copy2(build / 'compile_commands.json', output / 'compile_commands.json')
    commands = subprocess.check_output(['ninja', '-C', str(build), '-t', 'commands'], text=True)
    (output / 'ninja-commands.log').write_text(commands)
    for line in commands.splitlines():
        for token in shlex.split(line):
            path = Path(token)
            if path.is_file() and path.suffix == '.a':
                selected.add(path)
    for line in subprocess.check_output(['ninja', '-C', str(build), '-t', 'deps'], text=True).splitlines():
        path = Path(line.strip())
        if path.is_file() and source not in path.parents:
            selected.add(path)
    row['command'] = [str(output / 'fixedsys_owner_readability'), str(args.game_marlett), str(output / 'run')]
    row['timeout-seconds'] = 45
    rows.append(row)
    (art / 'cases.json').write_text(json.dumps(rows, indent=2) + '\n')
    print(profile, 'compiled', flush=True)

resources = {str(args.game_marlett): '3b6192eca4451835f4d634b0b6b0841da321ec65c1f3f1aa22d186da45787b9c',
             '/System/Library/Fonts/Courier.ttc': '76cdad2ba4e8774223ed6184f530570261f9f7fb9beb5c68368e09234c0273ec'}
assert args.game_marlett.stat().st_size == 6136
for tool in ['clang++', 'ld', 'ar']:
    selected.add(Path(subprocess.check_output(['xcrun', '--find', tool], text=True).strip()))
selected.update(Path(shutil.which(tool)) for tool in ['cmake', 'ninja', 'python3'])
pins = {str(p): sha(p) for p in sorted(selected)}
pins.update(resources)
pins.update({str(source / n): h for n, h in source_hashes.items()})
for row in rows:
    pins.update({str(art / row['profile'] / n): h for n, h in row['binary-hashes'].items()})


def check():
    actual = {name: sha(Path(name)) for name in pins}
    assert actual == pins, 'A source/image/dependency/resource changed; stop before next service execution'
    return actual


(art / 'pins-before.json').write_text(json.dumps(check(), indent=2) + '\n')
for row in rows:
    check()
    output = art / row['profile'] / 'run'
    output.mkdir()
    start = time.monotonic()
    with (output / 'stdout.log').open('wb') as stdout, (output / 'stderr.log').open('wb') as stderr:
        child = subprocess.Popen(row['command'], stdout=stdout, stderr=stderr, start_new_session=True)
        try:
            row['exit'] = child.wait(timeout=45)
            row['timed-out'] = False
        except subprocess.TimeoutExpired:
            os.killpg(child.pid, signal.SIGKILL)
            row['exit'] = child.wait(timeout=5)
            row['timed-out'] = True
    row['wall-seconds'] = time.monotonic() - start
    row['runtime-executed'] = True
    row['output-hashes'] = {p.name: sha(p) for p in sorted(output.iterdir()) if p.is_file()}
    (art / 'cases.json').write_text(json.dumps(rows, indent=2) + '\n')
    (art / 'pins-after.json').write_text(json.dumps(check(), indent=2) + '\n')
    print(row['profile'], 'exit', row['exit'], 'timeout', row['timed-out'], flush=True)
    assert row['exit'] == 0 and not row['timed-out'], 'Preserve failed run; no automatic retry'
