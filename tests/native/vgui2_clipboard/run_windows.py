#!/usr/bin/env python3
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--art', type=Path, required=True)
parser.add_argument('--work', type=Path, required=True)
args = parser.parse_args()
art = args.art.resolve()
art.mkdir(parents=True, exist_ok=True)
work = args.work.resolve()
work.mkdir(parents=True, exist_ok=True)
source = work / 'source'
shutil.copytree(args.source, source, dirs_exist_ok=True)
sdk = source / 'GoldSrc-SDK'
tests = source / 'tests/native/vgui2_clipboard'
records = []
for cpu in ['x86', 'x64', 'arm64']:
    setup = work / (cpu + '-setup.cmd')
    setup.write_text('@echo off\ncall "C:\\BuildTools\\2026\\Common7\\Tools\\VsDevCmd.bat" '
        '-no_logo -host_arch=arm64 -arch=' + cpu + ' >nul\nif errorlevel 1 exit /b 1\nset\n')
    result = subprocess.run(['cmd.exe', '/d', '/c', setup], capture_output=True, text=True, check=True)
    env = dict(os.environ)
    for line in result.stdout.splitlines():
        if '=' in line and not line.startswith('='):
            key, value = line.split('=', 1)
            env[key] = value
    compiler = Path(shutil.which('cl.exe', path=env.get('Path', env.get('PATH'))))
    (art / ('compiler-' + cpu + '.json')).write_text(json.dumps({'path': str(compiler),
        'sha256': hashlib.sha256(compiler.read_bytes()).hexdigest(), 'INCLUDE': env.get('INCLUDE'),
        'CL': env.get('CL', ''), '_CL_': env.get('_CL_', '')}, indent=2) + '\n')
    for mode in ['Release', 'Debug']:
        for name in ['clipboard_tests', 'reference_rows']:
            tag = cpu + '-' + mode + '-' + name
            build = work / tag
            build.mkdir(exist_ok=True)
            exe = build / 'pure_clipboard.exe'
            flags = ['/nologo', '/std:c++14', '/EHsc', '/fp:precise', '/W4', '/WX',
                '/D_CRT_SECURE_NO_WARNINGS', '/I' + str(sdk / 'vgui2/src')]
            flags += ['/O2', '/MD', '/DNDEBUG'] if mode == 'Release' else ['/Od', '/Zi', '/MDd', '/D_DEBUG']
            command = [str(compiler)] + flags + [str(tests / (name + '.cpp')),
                str(sdk / 'vgui2/src/NativeClipboard.cpp'), '/Fe' + str(exe)]
            result = subprocess.run(command, cwd=build, env=env, capture_output=True, timeout=90)
            (art / (tag + '-compile.log')).write_bytes(result.stdout + result.stderr)
            records.append({'context': tag, 'phase': 'compile', 'command': command, 'exit': result.returncode})
            (art / 'commands.json').write_text(json.dumps(records, indent=2) + '\n')
            assert result.returncode == 0, (tag, result.stdout, result.stderr)
            result = subprocess.run([str(exe)], cwd=build, env=env, capture_output=True, timeout=60)
            (art / (tag + '.stdout')).write_bytes(result.stdout)
            (art / (tag + '.stderr')).write_bytes(result.stderr)
            records.append({'context': tag, 'phase': 'pure-execute', 'command': [str(exe)], 'exit': result.returncode})
            (art / 'commands.json').write_text(json.dumps(records, indent=2) + '\n')
            assert result.returncode == 0, (tag, result.stdout, result.stderr)
            shutil.copy2(exe, art / (tag + '.exe'))
            print('PASS', tag, result.stdout.decode().splitlines()[0] if name == 'clipboard_tests' else 'authored rows', flush=True)
