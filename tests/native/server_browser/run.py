"""Freeze and test actual model/config source; no live Steam/provider invocation."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--repo', type=Path, required=True)
parser.add_argument('--packet', type=Path, required=True)
parser.add_argument('--sdl', type=Path, required=True)
args = parser.parse_args()
repo = args.repo.resolve()
packet = args.packet.resolve()
packet.mkdir(parents=True, exist_ok=False)
source = packet / 'source'
paths = []
for relative in ['src/GameUI/native', 'src/external/steamworks',
                 'tests/native/server_browser', 'src/external/SDL3/include',
                 'src/public', 'src/common']:
    for path in (repo / relative).rglob('*'):
        if path.is_file() and (path.suffix in ['.h', '.cpp', '.py', '.txt', '.json', '.patch', '.md'] or path.name == 'CMakeLists.txt'):
            paths.append(path)
for path in paths:
    target = source / path.relative_to(repo)
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(path, target)
pins = {str(path.relative_to(repo)): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}
commands = []
env = os.environ.copy()
if os.name != 'nt':
    env['DYLD_LIBRARY_PATH'] = str(args.sdl.resolve().parent)

def run(label, argv, allowed=(0,)):
    result = subprocess.run([str(value) for value in argv], capture_output=True, text=True, env=env)
    (packet / (label + '.log')).write_text(result.stdout + result.stderr)
    commands.append(dict(label=label, argv=[str(value) for value in argv], exit=result.returncode,
                         cwd=str(Path.cwd()), log=label + '.log'))
    (packet / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
    print(label, result.returncode, flush=True)
    if result.returncode not in allowed:
        raise RuntimeError(label)

for profile in ['release', 'debug', 'sanitizers']:
    build = packet / profile
    argv = ['cmake', '-S', source / 'tests/native/server_browser', '-B', build, '-G', 'Ninja',
            '-DCMAKE_BUILD_TYPE=' + ('Release' if profile == 'release' else 'Debug'),
            '-DCSE_SDL3_LIBRARY=' + str(args.sdl.resolve()),
            '-DCSE_BROWSER_SOURCE_ROOT=' + str(source)]
    if profile == 'sanitizers':
        argv += ['-DCSE_BROWSER_SANITIZERS=ON']
    run(profile + '-configure', argv)
    run(profile + '-build', ['cmake', '--build', build, '--parallel', '2'])
    run(profile + '-test', ['ctest', '--test-dir', build, '--output-on-failure', '-V'])
    executable = build / ('model_tests.exe' if os.name == 'nt' else 'model_tests')
    # Invariant violations must terminate before dispatch/mutation. SIGABRT on POSIX.
    if os.name != 'nt':
        for case in ['wrong-thread', 'wrong-thread-callback', 'live-destruction']:
            run(profile + '-' + case, [executable, case], allowed=(-6,))
outputs = {str(path.relative_to(packet)): hashlib.sha256(path.read_bytes()).hexdigest()
           for path in packet.rglob('*') if path.is_file() and
           (path.suffix in ['.o', '.a', '.d', '.json'] or path.name in ['model_tests', 'config_tests', 'favorite_adapter_tests', 'provider_probe',
               'address_formatter_little_tests', 'address_formatter_big_tests'])}
assert pins == {str(path.relative_to(repo)): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}
(packet / 'receipt.json').write_text(json.dumps(dict(sourcePins=pins, commands=commands,
    outputPins=outputs, sdl=str(args.sdl.resolve()), sdlSha256=hashlib.sha256(args.sdl.read_bytes()).hexdigest(),
    liveProviderRun=False), indent=2) + '\n')
