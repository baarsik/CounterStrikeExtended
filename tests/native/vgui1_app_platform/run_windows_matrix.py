"""Standalone fake-service execution only; no host UI, real clock, or provider."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--art', type=Path, required=True)
parser.add_argument('--work', type=Path, required=True)
args = parser.parse_args()
source = args.source.resolve()
art = args.art.resolve()
work = args.work.resolve()
art.mkdir(parents=True, exist_ok=True)
work.mkdir(parents=True, exist_ok=True)
# Unique owned source copy; no existing producer source/build tree is mutated.
local = work / 'source'
shutil.copytree(source, local, dirs_exist_ok=True)
tests = local / 'tests/native/vgui1_app_platform'
vgui = local / 'src/utils/vgui/lib-src/vgui'
header = work / 'public-declaring-header'
header.mkdir(exist_ok=True)
shutil.copy2(local / 'src/utils/vgui/include/VGUI_App.h', header / 'VGUI_App.h')
vsdev = r'C:\BuildTools\2026\Common7\Tools\VsDevCmd.bat'
rows = []

def run(name, command, env, cwd, expected=0):
    result = subprocess.run([str(x) for x in command], cwd=cwd, env=env, capture_output=True, timeout=180)
    (art / (name + '.log')).write_bytes(result.stdout + result.stderr)
    rows.append({'name': name, 'cwd': str(cwd), 'command': [str(x) for x in command], 'exit': result.returncode, 'expected': expected})
    (art / 'windows-commands.json').write_text(json.dumps(rows, indent=2) + '\n')
    assert (result.returncode == 0) if expected == 0 else (result.returncode != 0), rows[-1]
    return result

for cpu in ['x86', 'x64', 'arm64']:
    setup = work / ('setup-' + cpu + '.cmd')
    setup.write_text('@echo off\ncall \"' + vsdev + '\" -no_logo -host_arch=arm64 -arch=' + cpu + ' >nul\nif errorlevel 1 exit /b 1\nset\n')
    result = subprocess.run(['cmd.exe', '/d', '/c', str(setup)], capture_output=True, text=True, cwd=work)
    if result.returncode:
        (art / ('vsdev-' + cpu + '-failure.log')).write_text(result.stderr)
        raise RuntimeError('VsDevCmd failed: ' + result.stderr)
    env = dict(os.environ)
    for line in result.stdout.splitlines():
        if '=' in line and not line.startswith('='):
            key, value = line.split('=', 1)
            env[key] = value
    compiler = Path(shutil.which('cl.exe', path=env.get('Path', env.get('PATH'))))
    identity = {'path': str(compiler), 'sha256': hashlib.sha256(compiler.read_bytes()).hexdigest(),
        'vsdevcmd': vsdev, 'host_arch': 'arm64', 'target_arch': cpu,
        'CL': env.get('CL', ''), '_CL_': env.get('_CL_', ''), 'INCLUDE': env.get('INCLUDE', '')}
    (art / ('compiler-' + cpu + '.json')).write_text(json.dumps(identity, indent=2) + '\n')
    for config in ['Release', 'Debug']:
        name = cpu + '-' + config
        build = work / name
        build.mkdir(exist_ok=True)
        opts = ['/nologo', '/TP', '/std:c++14', '/EHsc', '/fp:precise', '/W4', '/WX',
            '/DVGUI_EXPORTS', '/DWIN32', '/D_WINDOWS', '/D_USRDLL', '/Dvgui_EXPORTS', '/I' + str(header),
            '/I' + str(vgui / 'include'), '/I' + str(vgui), '/I' + str(vgui / 'common'),
            '/I' + str(vgui / 'posix'), '/I' + str(tests)]
        opts += ['/O2', '/DNDEBUG', '/MT'] if config == 'Release' else ['/Od', '/Zi', '/D_DEBUG', '/MTd']
        files = [tests / 'component_tests.cpp', tests / 'controlled_dependencies.cpp', tests / 'controlled_surface.cpp',
            tests / 'actual_declaration_probe.cpp', vgui / 'App.cpp', vgui / 'posix/App.cpp', vgui / 'posix/VGui1AppPlatform.cpp']
        # Preserve real-provider dllexport compilation independently of the
        # visibility-neutral EXE linkage control. No existing header is edited.
        for index, file in enumerate(files):
            output = build / ('original-profile-' + str(index) + '.obj')
            run(name + '-original-profile-compile-' + str(index),
                [compiler] + opts + ['/c', file, '/Fo' + str(output)], env, build)
        private_opts = [x for x in opts if x != '/I' + str(header)]
        for index, file in enumerate([tests / 'actual_declaration_probe.cpp', vgui / 'App.cpp', vgui / 'posix/App.cpp']):
            output = build / ('original-private-' + str(index) + '.obj')
            run(name + '-original-private-compile-' + str(index),
                [compiler] + private_opts + ['/c', file, '/Fo' + str(output)], env, build)
        opts += ['/FI'  + str(tests / 'visibility_neutral_fixture.h')]
        objects = []
        for index, file in enumerate(files):
            output = build / ('part' + str(index) + '.obj')
            run(name + '-compile-' + str(index), [compiler] + opts + ['/c', file, '/Fo' + str(output)], env, build)
            objects.append(output)
        executable = build / 'app_component.exe'
        run(name + '-link', [compiler] + [x for x in opts if x != '/TP'] + objects + ['/Fe' + str(executable)], env, build)
        run(name + '-component', [executable], env, build)
        run(name + '-failure-children', [sys.executable, tests / 'failure_children.py', executable], env, build)
        run(name + '-extract-reference', [sys.executable, tests / 'extract_win_reference.py', vgui / 'win32/App.cpp', build / 'win_reference_methods.inc'], env, build)
        reference = build / 'win_reference.exe'
        run(name + '-reference-build', [compiler] + [x for x in opts if not x.startswith('/FI')] + ['/I' + str(build), tests / 'win_reference_tests.cpp', '/Fe' + str(reference)], env, build)
        run(name + '-reference', [reference], env, build)
        for executable in [executable, reference]:
            shutil.copy2(executable, art / (name + '-' + executable.name))
# Copy all input identities, not an environment dump.
hashes = {str(p.relative_to(local)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(local.rglob('*')) if p.is_file()}
(art / 'windows-input-hashes.json').write_text(json.dumps(hashes, indent=2) + '\n')
print('PASS six Windows CPU/config fake-service profiles + retained-body controls; no real platform service linked')
