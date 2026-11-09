#!/usr/bin/env python3
"""Actual whole-System compile and preprocessor controls; no linking or execution."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
from cpp_tokens import tokens

parser = argparse.ArgumentParser()
parser.add_argument('--art', type=Path, required=True)
parser.add_argument('--headers', type=Path, required=True)
parser.add_argument('--work', type=Path, required=True)
args = parser.parse_args()
art = args.art.resolve()
work = args.work.resolve()
work.mkdir(parents=True, exist_ok=True)
out = art / 'whole-tu'
out.mkdir(exist_ok=True)
sdk = args.headers / 'GoldSrc-SDK'
stage = work / 'GoldSrc-SDK'
source = stage / 'vgui2/src/System.cpp'
source.parent.mkdir(parents=True, exist_ok=True)
helper = stage / 'common/legacy_double_to_int.h'
helper.parent.mkdir(parents=True, exist_ok=True)
shutil.copy2(art / 'windows-source/GoldSrc-SDK/common/legacy_double_to_int.h', helper)
before = (art / 'System-before.cpp').read_bytes()
after = (art / 'System-after.cpp').read_bytes()
rows = []
for cpu in ['x86', 'x64', 'arm64']:
    setup = work / ('setup-' + cpu + '.cmd')
    setup.write_text('@echo off\ncall "C:\\BuildTools\\2026\\Common7\\Tools\\VsDevCmd.bat" '
        '-no_logo -host_arch=arm64 -arch=' + cpu + ' >nul\nif errorlevel 1 exit /b 1\nset\n')
    result = subprocess.run(['cmd.exe', '/d', '/c', setup], capture_output=True, text=True, check=True)
    env = dict(os.environ)
    for line in result.stdout.splitlines():
        if '=' in line and not line.startswith('='):
            key, value = line.split('=', 1)
            env[key] = value
    compiler = Path(shutil.which('cl.exe', path=env.get('Path', env.get('PATH'))))
    for mode in ['Release', 'Debug']:
        tag = cpu + '-' + mode
        build = work / tag
        build.mkdir(exist_ok=True)
        flags = ['/nologo', '/std:c++14', '/EHsc', '/fp:precise', '/W4', '/WX',
            '/DWIN32', '/D_WINDOWS', '/D_USRDLL', '/D_CRT_SECURE_NO_WARNINGS', '/D_CRT_NONSTDC_NO_DEPRECATE',
            '/I' + str(sdk / 'vgui2/src'), '/I' + str(sdk / 'vgui2/src/public'),
            '/I' + str(sdk / 'vgui2/src/vgui_surfacelib'), '/I' + str(sdk / 'public')]
        flags += ['/O2', '/MD', '/DNDEBUG'] if mode == 'Release' else ['/Od', '/Zi', '/MDd', '/D_DEBUG']
        source.write_bytes(after)
        dependency = build / 'dependencies.json'
        command = [str(compiler)] + flags + ['/c', str(source), '/Fo' + str(build / 'System.obj'),
            '/sourceDependencies', str(dependency)]
        result = subprocess.run(command, cwd=build, env=env, capture_output=True, timeout=90)
        (out / (tag + '-compile.log')).write_bytes(result.stdout + result.stderr)
        rows.append({'context': tag, 'phase': 'actual-whole-TU-compile', 'command': command, 'exit': result.returncode})
        (out / 'commands.json').write_text(json.dumps(rows, indent=2) + '\n')
        assert result.returncode == 0, (tag, result.stdout, result.stderr)
        data = json.loads(dependency.read_text(encoding='utf-8-sig'))
        assert data['Version'] == '1.2'
        shutil.copy2(dependency, out / (tag + '-raw-dependencies.json'))
        closure = [{'path': name, 'sha256': hashlib.sha256(Path(name).read_bytes()).hexdigest()}
            for name in [data['Data']['Source']] + data['Data']['Includes']]
        (out / (tag + '-closure.json')).write_text(json.dumps(closure, indent=2) + '\n')
        shutil.copy2(build / 'System.obj', out / (tag + '-System.obj'))
        if cpu == 'x86':
            reports = []
            variants = [('before', before), ('after', after),
                ('mutant-else', after.replace(b'\treturn (long)( GetCurrentTime() * 1000.0 );',
                    b'\treturn (long)( GetCurrentTime() * 1001.0 );'))]
            for variant, contents in variants:
                source.write_bytes(contents)
                command = [str(compiler)] + flags + ['/EP', str(source)]
                result = subprocess.run(command, cwd=build, env=env, capture_output=True, timeout=90)
                (out / (tag + '-' + variant + '.i')).write_bytes(result.stdout)
                (out / (tag + '-' + variant + '.log')).write_bytes(result.stderr)
                rows.append({'context': tag, 'phase': 'preprocess-' + variant, 'command': command,
                    'source_sha256': hashlib.sha256(contents).hexdigest(), 'exit': result.returncode})
                assert result.returncode == 0, (tag, variant, result.stderr)
                reports.append(tokens(result.stdout))
            assert reports[0] == reports[1] and reports[0] != reports[2], tag
            print('PASS', tag, 'actual Win32 selected whole-TU tokens identical; changed-else control rejected', flush=True)
        print('PASS', tag, 'actual System.cpp object compiled; no module link or execution', flush=True)
        (out / 'commands.json').write_text(json.dumps(rows, indent=2) + '\n')
source.write_bytes(after)
