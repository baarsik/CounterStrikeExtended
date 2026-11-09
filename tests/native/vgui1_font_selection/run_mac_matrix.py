#!/usr/bin/env python3
"""Actual owner/Font/Scheme execution; only the typed resource provider is controlled."""
import argparse
import hashlib
import json
import shlex
import shutil
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--art', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
art = args.art.resolve()
art.mkdir(parents=True, exist_ok=True)
rows = []

def run(name, phase, command):
    result = subprocess.run([str(value) for value in command], cwd=root, capture_output=True, text=True, timeout=180)
    (art / f'{name}-{phase}.log').write_text(result.stdout + result.stderr)
    rows.append({'name': name, 'phase': phase, 'command': [str(value) for value in command], 'exit': result.returncode})
    (art / 'mac-commands.json').write_text(json.dumps(rows, indent=2) + '\n')
    if result.returncode:
        raise RuntimeError(f'{name}/{phase}: {result.returncode}: {(result.stdout + result.stderr)[-4000:]}')
    return result.stdout

compiler = Path(subprocess.check_output(['xcrun', '--find', 'clang++'], text=True).strip())
(art / 'mac-compiler.json').write_text(json.dumps({'path': str(compiler),
    'resolved_path': str(compiler.resolve()), 'sha256': hashlib.sha256(compiler.read_bytes()).hexdigest(),
    'version': subprocess.check_output([compiler, '--version'], text=True)}, indent=2) + '\n')
for view in ('private', 'public'):
    for mode in ('release', 'debug', 'sanitizers'):
        name = f'{view}-{mode}'
        build = art / name
        command = ['cmake', '-S', root / 'tests/native/vgui1_font_selection', '-B', build, '-G', 'Ninja',
            '-DCMAKE_CXX_COMPILER=' + str(compiler), '-DCMAKE_BUILD_TYPE=' + ('Release' if mode == 'release' else 'Debug'),
            '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON', '-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0',
            '-DSELECTION_PUBLIC_HEADERS=' + ('ON' if view == 'public' else 'OFF'),
            '-DSELECTION_SANITIZERS=' + ('ON' if mode == 'sanitizers' else 'OFF')]
        if mode == 'sanitizers':
            command += ['-DCMAKE_CXX_FLAGS_DEBUG=-O1 -g']
        run(name, 'configure', command)
        run(name, 'build', ['cmake', '--build', build, '--parallel', '3'])
        run(name, 'ctest', ['ctest', '--test-dir', build, '--output-on-failure'])
        shutil.copyfile(build / 'compile_commands.json', art / f'{name}-compile-commands.json')
        # Preserve actual compiler-owned include closure after a successful compile.
        deps = run(name, 'deps', ['ninja', '-C', build, '-t', 'deps'])
        closure = {}
        for line in deps.splitlines():
            if not line.startswith('    '):
                continue
            path = Path(line.strip())
            if path.is_file():
                closure[str(path.resolve())] = hashlib.sha256(path.read_bytes()).hexdigest()
        assert closure, f'empty actual include closure: {name}'
        (art / f'{name}-closure.json').write_text(json.dumps(closure, indent=2, sort_keys=True) + '\n')
        print(f'PASS {name}: actual owner/Font/Scheme + expected fatal lease children', flush=True)
print('PASS 6 actual Mac configurations; controlled provider, no host font/UI services')
