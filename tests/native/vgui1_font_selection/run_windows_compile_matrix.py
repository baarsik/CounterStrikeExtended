#!/usr/bin/env python3
"""MSVC compile-only real declaring-context probes; native font owner stays Apple-only."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--work', type=Path, required=True)
parser.add_argument('--art', type=Path, required=True)
args = parser.parse_args()
work = args.work.resolve()
art = args.art.resolve()
work.mkdir(parents=True, exist_ok=True)
art.mkdir(parents=True, exist_ok=True)
root = work / 'source'
shutil.copytree(args.source, root, dirs_exist_ok=True)
vgui = root / 'src/utils/vgui/lib-src/vgui'
rows = []
for cpu in ('x86', 'x64', 'arm64'):
    setup = work / f'setup-{cpu}.cmd'
    setup.write_text('@echo off\ncall "C:\\BuildTools\\2026\\Common7\\Tools\\VsDevCmd.bat" -no_logo -host_arch=arm64 -arch='
        + cpu + ' >nul\nif errorlevel 1 exit /b 1\nset\n')
    result = subprocess.run(['cmd.exe', '/d', '/c', setup], capture_output=True, text=True, cwd=work, check=True)
    env = dict(os.environ)
    for line in result.stdout.splitlines():
        if '=' in line and not line.startswith('='):
            key, value = line.split('=', 1)
            env[key] = value
    compiler = Path(shutil.which('cl.exe', path=env.get('Path', env.get('PATH'))))
    (art / f'compiler-{cpu}.json').write_text(json.dumps({'path': str(compiler),
        'sha256': hashlib.sha256(compiler.read_bytes()).hexdigest(), 'INCLUDE': env.get('INCLUDE', ''),
        'CL': env.get('CL', ''), '_CL_': env.get('_CL_', '')}, indent=2) + '\n')
    for mode in ('Release', 'Debug'):
        flags = ['/nologo', '/std:c++14', '/EHsc', '/fp:precise', '/W4', '/WX', '/DWIN32', '/D_WINDOWS',
            '/D_USRDLL', '/DVGUI_EXPORTS']
        flags += ['/O2', '/MT', '/DNDEBUG'] if mode == 'Release' else ['/Od', '/Zi', '/MTd', '/D_DEBUG']
        for view in ('private', 'public'):
            name = f'{cpu}-{view}-{mode}'
            build = work / name
            build.mkdir(exist_ok=True)
            includes = [vgui / 'include', vgui, vgui / 'common', root / 'src/engine']
            if view == 'public':
                includes.insert(0, root / 'src/utils/vgui/include')
            for index, file in enumerate((root / 'src/engine/VGui1NativeFontSelection.cpp',
                root / 'tests/native/vgui1_font_selection/actual_declaration_probe.cpp')):
                dep = art / f'{name}-{index}-dependencies.json'
                command = [str(compiler)] + flags + ['/I' + str(path) for path in includes] + ['/c', str(file),
                    '/sourceDependencies', str(dep), '/Fo' + str(build / f'part-{index}.obj')]
                result = subprocess.run(command, cwd=build, env=env, capture_output=True, timeout=120)
                (art / f'{name}-{index}.log').write_bytes(result.stdout + result.stderr)
                rows.append({'context': name, 'file': str(file.relative_to(root)), 'command': command,
                    'exit': result.returncode, 'execution': False})
                (art / 'windows-commands.json').write_text(json.dumps(rows, indent=2) + '\n')
                assert result.returncode == 0, rows[-1]
                report = json.loads(dep.read_text(encoding='utf-8-sig'))
                closure = [Path(report['Data']['Source'])] + [Path(value) for value in report['Data']['Includes']]
                assert len(closure) > 5, 'unknown/empty compiler include closure'
                (art / f'{name}-{index}-closure.json').write_text(json.dumps({str(path):
                    hashlib.sha256(path.read_bytes()).hexdigest() for path in closure}, indent=2) + '\n')
            print(f'PASS {name}: actual new TU + real header type assertions; compile only', flush=True)
(art / 'windows-input-hashes.json').write_text(json.dumps({str(path.relative_to(root)):
    hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted(root.rglob('*')) if path.is_file()}, indent=2) + '\n')
print('PASS 12 MSVC header profiles/24 compiles; no module/provider execution')
