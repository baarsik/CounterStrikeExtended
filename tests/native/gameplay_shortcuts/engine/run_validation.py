"""Run actual owner/dispatch/lifecycle and retained mouse/client controls, without native UI."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--repo', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
repo = args.repo.resolve()
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=False)
include = repo / 'build/dedicated-graph-release/dependencies/SDL3/source/SDL3-3.4.20/include'
config = repo / 'build/dedicated-graph-release/dependencies/SDL3/upstream/include-config-release/build_config'
commands = []

def run(name, argv, expected=0):
    result = subprocess.run([str(value) for value in argv], cwd=repo, capture_output=True, text=True, timeout=300)
    (out / (name + '.log')).write_text(result.stdout + result.stderr)
    commands.append(dict(name=name, argv=[str(value) for value in argv], cwd=str(repo), exit=result.returncode,
                         expected=expected))
    (out / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
    print(name, result.returncode, flush=True)
    assert result.returncode == expected, name

for profile, build_type, sanitizers in [('release', 'Release', False), ('debug', 'Debug', False),
                                       ('sanitizers', 'Debug', True)]:
    for suite, path, option in [('engine', 'gameplay_shortcuts/engine', 'CSE_SHORTCUT_ENGINE_SANITIZERS'),
                                ('mouse', 'native_mouse_window', 'CSE_NATIVE_MOUSE_SANITIZERS'),
                                ('client', 'client_input', 'CSE_CLIENT_INPUT_SANITIZERS')]:
        name = profile + '-' + suite
        build = out / name
        run(name + '-configure', ['cmake', '-S', repo / 'tests/native' / path, '-B', build, '-G', 'Ninja',
            '-DCMAKE_BUILD_TYPE=' + build_type, '-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0',
            '-DCSE_SDL_INCLUDE=' + str(include), '-DCSE_SDL_CONFIG=' + str(config),
            '-D' + option + '=' + ('ON' if sanitizers else 'OFF')])
        run(name + '-build', ['cmake', '--build', build, '--parallel', '2'])
        run(name + '-test', ['ctest', '--test-dir', build, '--output-on-failure'])
        if suite == 'engine':
            for binary in ['shortcut_engine_hw', 'shortcut_engine_sw', 'shortcut_lifecycle_hw', 'shortcut_lifecycle_sw']:
                run(name + '-' + binary, [build / binary])
        if suite == 'mouse':
            for failure in ['setter', 'postcheck']:
                run(name + '-startup-' + failure, [build / 'native_mouse_lifecycle_tests',
                    '--startup-fail-' + failure], 21)
            run(name + '-naive-red', [build / 'native_mouse_naive_red'], 17)
source_names = subprocess.check_output(['git', 'diff', '--name-only'], cwd=repo, text=True).splitlines()
source_names += [str(p.relative_to(repo)) for p in (repo / 'tests/native/gameplay_shortcuts/engine').glob('*') if p.is_file()]
source_names += ['tests/native/gameplay_shortcuts/build_engine_packet.py']
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
(out / 'source-pins.json').write_text(json.dumps({name: sha(repo / name) for name in sorted(set(source_names))}, indent=2)+'\n')
# Unchanged client mathematics, commands/bindings and AppActivate audio/career source owners.
unchanged = ['GoldSrc-SDK/cl_dll/inputw32.cpp', 'GoldSrc-SDK/cstrike/cl_dll/inputw32.cpp',
             'GoldSrc-SDK/engine/cmd.cpp', 'GoldSrc-SDK/engine/keys.cpp', 'GoldSrc-SDK/engine/cd.cpp']
proof = {}
for name in unchanged:
    before = subprocess.check_output(['git', 'show', '8369c9da1581ff8947bc929b085a2e6aac577b09:' + name], cwd=repo)
    assert before == (repo / name).read_bytes()
    proof[name] = sha(repo / name)
(out / 'unchanged-controls.json').write_text(json.dumps(proof, indent=2) + '\n')
