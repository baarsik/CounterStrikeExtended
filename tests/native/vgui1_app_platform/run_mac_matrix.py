"""Scoped actual-App fake-service matrix, never a real host service launch."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--art', type=Path, required=True)
args = parser.parse_args()
art = args.art.resolve()
root = Path(__file__).resolve().parents[3]
source = root / 'tests/native/vgui1_app_platform'
vgui = root / 'GoldSrc-SDK/utils/vgui/lib-src/vgui'
art.mkdir(parents=True, exist_ok=True)
rows = []

def run(name, command, expected=0, cwd=root):
    result = subprocess.run([str(x) for x in command], cwd=cwd, capture_output=True, text=True)
    (art / (name + '.log')).write_text(result.stdout + result.stderr)
    row = {'name': name, 'cwd': str(cwd), 'command': [str(x) for x in command], 'exit': result.returncode, 'expected': expected}
    rows.append(row)
    (art / 'mac-commands.json').write_text(json.dumps(rows, indent=2) + '\n')
    assert (result.returncode == 0) if expected == 0 else (result.returncode != 0), row
    return result

run('source-green', ['python3', source / 'test_source_boundary.py'])
for name, config, sanitizers in [('final-release', 'Release', False), ('final-debug', 'Debug', False), ('final-sanitizers', 'Debug', True)]:
    build = art / name
    run(name + '-configure', ['cmake', '-S', source, '-B', build, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=' + config,
        '-DCMAKE_CXX_COMPILER=/usr/bin/clang++', '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON', '-DCSE_APP_SANITIZERS=' + ('ON' if sanitizers else 'OFF')])
    run(name + '-build', ['cmake', '--build', build, '--parallel', '2'])
    run(name + '-tests', ['ctest', '--test-dir', build, '--output-on-failure'])
    run(name + '-undefined', ['nm', '-u', build / 'app_component'])
    symbols = (art / (name + '-undefined.log')).read_text()
    assert all(token not in symbols for token in ['_mach_timebase_info', '_mach_continuous_time', '_SDL_', '_CFPreferences', '_CTFont'])
private = vgui / 'include/VGUI_App.h'
control = art / 'CONTROL-ONLY-private-int/VGUI_App.h'
control.parent.mkdir(parents=True, exist_ok=True)
text = private.read_text()
for name in ('setRegistryString', 'getRegistryString', 'setRegistryInteger', 'getRegistryInteger'):
    old = 'virtual bool ' + name
    assert text.count(old) == 1
    text = text.replace(old, 'virtual int ' + name)
control.write_text(text)
(art / 'private-int-negative-control.json').write_text(json.dumps({'control_only': True, 'production_edited': False,
    'source': str(private), 'source_sha256': hashlib.sha256(private.read_bytes()).hexdigest(),
    'copy_sha256': hashlib.sha256(control.read_bytes()).hexdigest(), 'negative_control': 'four bool registry returns -> int'}, indent=2) + '\n')
for name, header, expected in [('private-original', private, 0), ('CONTROL-private-int-REJECT', control, 1)]:
    build = art / name
    run(name + '-configure', ['cmake', '-S', source, '-B', build, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', '-DCSE_APP_DECLARATION=' + str(header)])
    run(name + '-build', ['cmake', '--build', build, '--parallel', '2'], expected)
    if expected == 0:
        run(name + '-tests', ['ctest', '--test-dir', build, '--output-on-failure'])

# Replay real current owner recipes. Source/output substitutions and the verbatim
# public-App overlay controls are explicit; all other flags remain byte-for-byte tokens.
context_rows = []
for config in ['release', 'debug']:
    db_path = art.parent / ('macos-runtime-policy-' + config) / 'compile_commands.json'
    database = json.loads(db_path.read_text())
    owner = next(x for x in database if x['file'] == str(vgui / 'App.cpp'))
    base = shlex.split(owner['command'])
    base = base[:base.index('-o')]
    overlay = art / 'final-release/declaring-header'
    variants = [('provider-shared-public', vgui / 'App.cpp', True), ('platform-App-public', vgui / 'posix/App.cpp', True),
        ('platform-App-private', vgui / 'posix/App.cpp', False), ('adapter', vgui / 'posix/VGui1AppPlatform.cpp', False),
        ('real-clock-COMPILE-ONLY', vgui / 'posix/VGui1AppClock.cpp', False)]
    for name, file, public in variants:
        target = art / 'owner-contexts' / (config + '-' + name)
        target.parent.mkdir(exist_ok=True)
        command = base + (['-I' + str(overlay)] if False else [])
        # The overlay must precede the unchanged private header directory.
        command = base[:1] + (['-I' + str(overlay)] if public else []) + base[1:]
        command += ['-I' + str(vgui / 'posix'), '-MD', '-MF', str(target) + '.d', '-c', str(file), '-o', str(target) + '.o']
        expected = 1 if 'REJECT' in name else 0
        run(config + '-' + name, command, expected, Path(owner['directory']))
        context_rows.append({'name': config + '-' + name, 'owner': owner, 'actual_source': str(file), 'public_overlay': public, 'expected': expected})
    for owner in [x for x in database if x['file'].endswith('/engine/vgui_intwrap.cpp')]:
        command = shlex.split(owner['command'])
        command = command[:command.index('-o')]
        target = art / 'owner-contexts' / (config + '-' + ('hw' if 'targets/hw_engine/' in owner['output'] else 'swds') + '-actual-CDummyApp.o')
        command += ['-c', owner['file'], '-o', str(target)]
        # Actual whole owner TU: any unrelated diagnostics remain visible.
        result = subprocess.run(command, cwd=owner['directory'], capture_output=True, text=True)
        (target.with_suffix('.log')).write_text(result.stdout + result.stderr)
        context_rows.append({'name': target.stem, 'owner': owner, 'command': command, 'exit': result.returncode, 'context': 'unchanged whole actual owner TU; no execution'})
(art / 'owner-contexts.json').write_text(json.dumps(context_rows, indent=2) + '\n')
compiler = Path('/usr/bin/clang++')
(art / 'mac-compiler.json').write_text(json.dumps({'path': str(compiler), 'resolved': str(compiler.resolve()),
    'sha256': hashlib.sha256(compiler.read_bytes()).hexdigest(), 'version': subprocess.check_output([compiler, '--version'], text=True)}, indent=2) + '\n')
print('PASS actual-App Mac component matrix, both real declarations and copied-int rejection; owner TUs recorded separately')
