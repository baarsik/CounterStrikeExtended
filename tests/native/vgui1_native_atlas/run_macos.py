"""Pure actual-owner/dylib/metadata matrix; never real fonts, SDL or GPU."""
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
parser.add_argument('--before', required=True, type=Path)
parser.add_argument('--source-root', type=Path, required=True)
args = parser.parse_args()
repo = Path(__file__).resolve().parents[3]
args.art.mkdir(exist_ok=False)
assert 'ASAN_OPTIONS' not in os.environ and 'UBSAN_OPTIONS' not in os.environ
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
rows = []
owned_paths = [args.source_root / 'src/engine' / name for name in [
    'VGui1NativeRuntime.h', 'VGui1NativeRuntime.cpp', 'VGui1NativeAtlas.h', 'VGui1NativeAtlas.cpp',
    'VGUI_EngineSurfaceWrap.cpp',
    'vgui_intwrap.cpp', 'vgui_EngineSurface.cpp', 'vgui_EngineSurfaceHW.cpp', 'vgui_EngineSurfaceSW.cpp',
    'vgui_EngineSurface.h', 'sys_sdlwind.cpp']]
owned_paths += [repo / 'src/engine/VGui1NativeFontSelection.h',
                repo / 'src/engine/VGui1NativeFontSelection.cpp']
owned_paths += [path for path in Path(__file__).parent.iterdir() if path.is_file()]
initial = {str(path): sha(path) for path in owned_paths}
(args.art / 'source-pins-before.json').write_text(json.dumps(initial, indent=2) + '\n')


def execute(label, command, cwd=repo, expected=0):
    with (args.art / (label + '.log')).open('wb') as log:
        result = subprocess.run([str(value) for value in command], cwd=cwd, stdout=log,
                                stderr=subprocess.STDOUT, timeout=60)
    rows.append({'label': label, 'command': [str(value) for value in command], 'cwd': str(cwd),
                 'exit': result.returncode, 'expected': expected})
    (args.art / 'commands.json').write_text(json.dumps(rows, indent=2) + '\n')
    assert result.returncode == expected, (label, result.returncode)


for profile, config, sanitizers in [('release', 'Release', False), ('debug', 'Debug', False),
                                    ('sanitized', 'Debug', True)]:
    build = repo / ('build/vgui1-native-atlas-v2-' + profile)
    execute(profile + '-configure', ['cmake', '-S', Path(__file__).parent, '-B', build, '-G', 'Ninja',
        '-DCMAKE_BUILD_TYPE=' + config, '-DCMAKE_OSX_ARCHITECTURES=arm64',
        '-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0', '-DCSE_NATIVE_SOURCE_ROOT=' + str(args.source_root),
        '-DCSE_ATLAS_SANITIZERS=' + ('ON' if sanitizers else 'OFF')])
    execute(profile + '-build', ['cmake', '--build', build, '--parallel', '2'])
    execute(profile + '-tests', ['ctest', '--test-dir', build, '--output-on-failure', '--timeout', '45'])
    shutil.copy2(build / 'compile_commands.json', args.art / (profile + '-compile-commands.json'))
    shutil.copytree(build / 'retirement', args.art / (profile + '-extracted-retirement'))
    for name in ['atlas_owned_host', 'libatlas_root_owner.dylib', 'libatlas_font_owner.dylib',
                 'retirement_hw', 'retirement_sw', 'embedded_adapter']:
        image = build / name
        shutil.copy2(image, args.art / (profile + '-' + name))
        execute(profile + '-' + name + '-build-version', ['xcrun', 'vtool', '-show-build', image])
        execute(profile + '-' + name + '-machine', ['file', image])
        execute(profile + '-' + name + '-imports', ['otool', '-L', image])
        assert 'arm64' in (args.art / (profile + '-' + name + '-machine.log')).read_text()
        assert 'minos 14.0' in (args.art / (profile + '-' + name + '-build-version.log')).read_text()

# Actual old/new recipes for all changed existing owner TUs. New private files
# are explicitly DERIVED from the corresponding real vgui_intwrap owner recipe.
selected = {'vgui_enginesurfacewrap.cpp', 'vgui_intwrap.cpp', 'vgui_enginesurface.cpp',
            'vgui_enginesurfacehw.cpp', 'vgui_enginesurfacesw.cpp', 'sys_sdlwind.cpp'}
art_root = args.before.parents[1]
for config in ['release', 'debug']:
    database_path = art_root / ('macos-runtime-policy-' + config) / 'compile_commands.json'
    database = json.loads(database_path.read_text())
    for entry in database:
        path = Path(entry['file'])
        if path.name.lower() not in selected:
            continue
        owner = 'swds' if '/swds/' in entry['output'] else 'hw' if '/hw_engine/' in entry['output'] else None
        if not owner:
            continue
        original = shlex.split(entry['command'])
        label = config + '-' + owner + '-' + path.stem
        before = args.before / 'engine' / path.name
        if not before.exists() and path.name == 'vgui_EngineSurface.cpp':
            before = args.before / 'engine/VGUI_EngineSurface.cpp'
        assert before.is_file(), before
        for phase, source in [('before', before), ('after', args.source_root / 'src/engine' / path.name)]:
            command = original.copy()
            command[command.index('-c') + 1] = str(source)
            command[command.index('-o') + 1] = str(args.art / (label + '-' + phase + '.o'))
            execute(label + '-' + phase, command, cwd=entry['directory'])
        if path.name == 'vgui_intwrap.cpp':
            for name in ['VGui1NativeRuntime.cpp', 'VGui1NativeAtlas.cpp']:
                command = original.copy()
                command[command.index('-c') + 1] = str(args.source_root / 'src/engine' / name)
                command.append('-fexceptions') # Explicit proposed per-file Sync policy only.
                command[command.index('-o') + 1] = str(args.art / (config + '-' + owner + '-derived-' + name + '.o'))
                execute(config + '-' + owner + '-derived-' + name, command, cwd=entry['directory'])
    (args.art / (config + '-database-identity.json')).write_text(json.dumps({
        'path': str(database_path), 'sha256': sha(database_path)}, indent=2) + '\n')

final = {str(path): sha(path) for path in owned_paths}
(args.art / 'source-pins-after.json').write_text(json.dumps(final, indent=2) + '\n')
assert final == initial, 'source changed during final gate'
