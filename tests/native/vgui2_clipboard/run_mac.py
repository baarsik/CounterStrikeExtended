#!/usr/bin/env python3
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--repo', type=Path, required=True)
parser.add_argument('--art', type=Path, required=True)
args = parser.parse_args()
repo = args.repo.resolve()
art = args.art.resolve()
out = art / 'mac'
out.mkdir(parents=True, exist_ok=True)
source = repo / 'GoldSrc-SDK/vgui2/src'
tests = repo / 'tests/native/vgui2_clipboard'
commands = []
for mode, flags in [('Release', ['-O2', '-DNDEBUG']), ('Debug', ['-O0', '-g', '-D_DEBUG']),
        ('Sanitizers', ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer'])]:
    for name in ['clipboard_tests', 'reference_rows']:
        exe = out / (mode + '-' + name)
        command = ['/usr/bin/clang++', '-std=c++14', '-arch', 'arm64', '-mmacosx-version-min=14.0',
            '-Wall', '-Wextra', '-Werror', '-Wconversion', '-pedantic', '-I' + str(source),
            str(tests / (name + '.cpp')), str(source / 'NativeClipboard.cpp'), '-o', str(exe)] + flags
        result = subprocess.run(command, capture_output=True, timeout=90)
        tag = mode + '-' + name
        (out / (tag + '-compile.log')).write_bytes(result.stdout + result.stderr)
        commands.append({'phase': 'compile', 'command': command, 'exit': result.returncode})
        (out / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
        assert result.returncode == 0, result.stderr
        result = subprocess.run([str(exe)], capture_output=True, timeout=60)
        (out / (tag + '.stdout')).write_bytes(result.stdout)
        (out / (tag + '.stderr')).write_bytes(result.stderr)
        commands.append({'phase': 'pure-execute', 'command': [str(exe)], 'exit': result.returncode})
        (out / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
        assert result.returncode == 0, result.stderr
        print('PASS', tag, result.stdout.decode().splitlines()[0] if name == 'clipboard_tests' else 'authored rows')
compiler = Path('/usr/bin/clang++').resolve()
(out / 'compiler.json').write_text(json.dumps({'path': str(compiler),
    'sha256': hashlib.sha256(compiler.read_bytes()).hexdigest(), 'version': subprocess.run(
        [str(compiler), '--version'], capture_output=True, text=True, check=True).stdout}, indent=2) + '\n')
