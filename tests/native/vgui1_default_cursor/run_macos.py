"""Complete actual constructor fixture and existing owning recipes; no UI/services."""
import argparse
import hashlib
import json
import shlex
import shutil
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--art', type=Path, required=True)
parser.add_argument('--before', type=Path, required=True)
args = parser.parse_args()
repo = Path(__file__).resolve().parents[3]
args.art.mkdir(exist_ok=False)
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
records = []


def execute(label, command, cwd=repo, expected=0):
    with (args.art / (label + '.log')).open('wb') as stream:
        result = subprocess.run([str(x) for x in command], cwd=cwd, stdout=stream,
                                stderr=subprocess.STDOUT, timeout=90)
    records.append({'label': label, 'command': [str(x) for x in command], 'cwd': str(cwd),
                    'exit': result.returncode, 'expected': expected})
    (args.art / 'commands.json').write_text(json.dumps(records, indent=2) + '\n')
    assert result.returncode == expected, (label, result.returncode)


source = repo / 'src/utils/vgui/lib-src/vgui/DefaultCursor.cpp'
include = repo / 'src/utils/vgui/lib-src/vgui/include'
for phase, path in [('before', args.before), ('after', source)]:
    ast = args.art / (phase + '-ast.json')
    command = ['xcrun', 'clang++', '-std=c++14', '-arch', 'arm64', '-mmacosx-version-min=14.0',
               '-I', str(include), '-Xclang', '-ast-dump=json', '-fsyntax-only', str(path)]
    with ast.open('wb') as out, (args.art / (phase + '-ast.stderr')).open('wb') as err:
        result = subprocess.run(command, stdout=out, stderr=err, timeout=45)
    records.append({'label': phase + '-ast', 'command': command, 'exit': result.returncode, 'expected': 0})
    assert result.returncode == 0
    execute(phase + '-constructor-init', ['python3', Path(__file__).with_name('check_constructor_init.py'), ast],
            expected=1 if phase == 'before' else 0)

for profile, config, sanitize in [('release', 'Release', False), ('debug', 'Debug', False),
                                   ('sanitized', 'Debug', True)]:
    build = repo / ('build/vgui1-default-cursor-final-' + profile)
    execute(profile + '-configure', ['cmake', '-S', Path(__file__).parent, '-B', build, '-G', 'Ninja',
        '-DCMAKE_BUILD_TYPE=' + config, '-DCMAKE_OSX_ARCHITECTURES=arm64',
        '-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0', '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
        '-DCSE_CURSOR_SANITIZERS=' + ('ON' if sanitize else 'OFF')])
    execute(profile + '-build', ['cmake', '--build', build, '--parallel', '2'])
    execute(profile + '-tests', ['ctest', '--test-dir', build, '--output-on-failure'])
    image = build / 'default_cursor'
    for mode in ['ordinary', 'none', 'all']:
        execute(profile + '-' + mode, [image, '--' + mode])
    shutil.copy2(image, args.art / (profile + '-default_cursor'))
    shutil.copy2(build / 'compile_commands.json', args.art / (profile + '-fixture-commands.json'))
    execute(profile + '-image-identity', ['xcrun', 'vtool', '-show-build', image])
    execute(profile + '-image-machine', ['file', image])
    execute(profile + '-imports', ['otool', '-L', image])
    assert (args.art / (profile + '-image-machine.log')).read_text().find('arm64') >= 0
    assert (args.art / (profile + '-image-identity.log')).read_text().find('minos 14.0') >= 0

# Reuse exactly selected vgui source commands. Redirect only source/output/depfile,
# leaving architecture/minimum OS, forced header, FP/EH/RTTI and warnings intact.
for config in ['release', 'debug']:
    database = args.art.parents[1] / ('macos-runtime-policy-' + config) / 'compile_commands.json'
    entries = json.loads(database.read_text())
    entries = [entry for entry in entries if Path(entry['file']) == source]
    assert len(entries) == 1
    entry = entries[0]
    original = shlex.split(entry['command'])
    for phase, path in [('before', args.before), ('after', source)]:
        command = original.copy()
        command[command.index('-o') + 1] = str(args.art / (config + '-' + phase + '-whole-tu.o'))
        command[command.index('-c') + 1] = str(path)
        execute(config + '-' + phase + '-whole-tu', command, cwd=entry['directory'])
    (args.art / (config + '-owning-recipe.json')).write_text(json.dumps({
        'database': str(database), 'database_sha256': sha(database), 'entry': entry}, indent=2) + '\n')

# Source-selected Windows bodies use the original private declaring headers.
# This is local Clang preprocessing, not a new MSVC invocation or Windows runtime.
for phase, path in [('before', args.before), ('after', source)]:
    execute('windows-selected-' + phase, ['xcrun', 'clang++', '-E', '-P', '-std=c++14',
        '-D_WIN32', '-DVGUI_EXPORTS', '-I', include, path])
assert (args.art / 'windows-selected-before.log').read_bytes() == (args.art / 'windows-selected-after.log').read_bytes()
assert all((args.art / (profile + '-ordinary.log')).read_bytes()
           == (args.art / 'release-ordinary.log').read_bytes() for profile in ['debug', 'sanitized'])
(args.art / 'summary.json').write_text(json.dumps({'profiles': 3, 'tests_per_profile': 3,
    'all_expected_exits': True, 'whole_owning_compiles_before_after': 4,
    'windows_selected_preprocessed_bytes_equal': True, 'fresh_windows_compilation': False,
    'provider_ui_gpu_sdl_calls': 0, 'fixture_cleanup_only': True}, indent=2) + '\n')
