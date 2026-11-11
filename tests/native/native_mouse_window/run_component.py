from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import time

parser = argparse.ArgumentParser()
parser.add_argument('--repo', type=Path, required=True)
parser.add_argument('--sdl-include', type=Path, required=True)
parser.add_argument('--build', type=Path, required=True)
parser.add_argument('--evidence', type=Path, required=True)
parser.add_argument('--config', choices=['Release', 'Debug'], required=True)
parser.add_argument('--sdl-config', type=Path)
parser.add_argument('--sanitizers', action='store_true')
args = parser.parse_args()
args.evidence.mkdir(parents=True, exist_ok=False)
records = []

def run(name, argv, expected=0):
    start = time.monotonic()
    result = subprocess.run([str(x) for x in argv], capture_output=True, text=True, timeout=300)
    (args.evidence / (name + '.log')).write_text(result.stdout + result.stderr)
    record = dict(name=name, argv=[str(x) for x in argv], exit=result.returncode, expected=expected,
                  seconds=time.monotonic() - start)
    records.append(record)
    (args.evidence / 'receipt.json').write_text(json.dumps(dict(commands=records, success=False), indent=2) + '\n')
    assert result.returncode == expected, record

configure = ['cmake', '-S', args.repo / 'tests/native/native_mouse_window', '-B', args.build, '-G', 'Ninja',
    '-DCMAKE_BUILD_TYPE=' + args.config, '-DCSE_SDL_INCLUDE=' + str(args.sdl_include),
    '-DCSE_NATIVE_MOUSE_SANITIZERS=' + ('ON' if args.sanitizers else 'OFF')]
if os.name != 'nt':
    configure.append('-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0')
    if args.sdl_config:
        configure.append('-DCSE_SDL_CONFIG=' + str(args.sdl_config))
run('configure', configure)
run('build', ['cmake', '--build', args.build, '--parallel', '2'])
suffix = '.exe' if os.name == 'nt' else ''
run('old-helper-red', [args.build / ('native_mouse_naive_red' + suffix)], 17)
for name in ['window', 'platform', 'caller', 'lifecycle']:
    run(name, [args.build / ('native_mouse_' + name + '_tests' + suffix)])
if int((args.build / 'pointer-bytes.txt').read_text()) == 8:
    for failure in ['setter', 'postcheck']:
        run('lifecycle-fatal-' + failure, [args.build / ('native_mouse_lifecycle_tests' + suffix),
            '--startup-fail-' + failure], 21)
run('ctest', ['ctest', '--test-dir', args.build, '--output-on-failure'])
images = []
for path in sorted(args.build.glob('native_mouse_*')):
    if not path.is_file() or path.suffix not in ['', '.exe']:
        continue
    out = args.evidence / 'images' / path.name
    out.parent.mkdir(exist_ok=True)
    shutil.copy2(path, out)
    images.append(dict(path=str(path), relative=out.relative_to(args.evidence).as_posix(),
        bytes=path.stat().st_size, sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
shutil.copytree(args.build / 'callers', args.evidence / 'imported-caller-lifecycle', dirs_exist_ok=True)
(args.evidence / 'compile_commands.json').write_bytes((args.build / 'compile_commands.json').read_bytes())
(args.evidence / 'receipt.json').write_text(json.dumps(dict(config=args.config, sanitizers=args.sanitizers,
    commands=records, images=images, success=True, injected_services_only=True, physical_window_acceptance=False,
    provider_or_game_executed=False), indent=2) + '\n')
print(args.config + ': component suites pass; old helper compile0/run17', flush=True)
