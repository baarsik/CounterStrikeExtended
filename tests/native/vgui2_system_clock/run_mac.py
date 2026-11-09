#!/usr/bin/env python3
"""Pure clock execution; the real POSIX reader is compiled but never linked/run."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--repo', type=Path, required=True)
parser.add_argument('--art', type=Path, required=True)
args = parser.parse_args()
repo = args.repo.resolve()
art = args.art.resolve()
out = art / 'mac'
out.mkdir(parents=True, exist_ok=True)
tests = repo / 'tests/native/vgui2_system_clock'
source = repo / 'GoldSrc-SDK/vgui2/src'
compiler = Path('/usr/bin/clang++')
commands = []

def run(command, tag, cwd=repo, accepted=0):
    result = subprocess.run([str(item) for item in command], cwd=cwd, capture_output=True, timeout=90)
    (out / (tag + '.stdout')).write_bytes(result.stdout)
    (out / (tag + '.stderr')).write_bytes(result.stderr)
    commands.append({'tag': tag, 'command': [str(item) for item in command], 'cwd': str(cwd), 'exit': result.returncode})
    (out / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
    assert result.returncode == accepted, (tag, result.stderr.decode(errors='replace'))
    return result

cases = [('normal', value) for value in [1, 1000, 1000000, 2000000, 2000001, 10000000,
    1000000000, 4294967296, 4294967296000000]] + [('seeded', 1000000000), ('same-time', 1000000), ('millis', 1000000)]
for mode, flags in [('Release', ['-O2', '-DNDEBUG']), ('Debug', ['-O0', '-g', '-D_DEBUG']),
        ('Sanitizers', ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer'])]:
    exe = out / mode
    command = [compiler, '-std=c++14', '-Wall', '-Wextra', '-Werror', '-Wconversion', '-pedantic',
        '-ffp-model=precise', '-ffp-contract=off', '-Wno-overriding-option', '-I' + str(source), tests / 'clock_fixture.cpp', '-o', exe] + flags
    run(command, mode + '-compile')
    for scenario, frequency in cases:
        run([exe, scenario, str(frequency)], mode + '-' + scenario + '-' + str(frequency))
    result = run([exe, 'boundaries'], mode + '-boundaries')
    for row in map(json.loads, result.stdout.splitlines()):
        expected = row['seconds'] * 1000000000 + row['nanos']
        valid = 0 <= expected <= 2**64 - 1
        assert row['valid'] == valid and row['encoded'] == (expected if valid else 987), row
    symbols = run(['/usr/bin/nm', '-u', exe], mode + '-undefined-symbols').stdout
    assert b'clock_gettime' not in symbols and b'ReadMonotonicCounter' not in symbols
    print('PASS', mode, 'pure component cases + independent integer nanosecond oracle; real reader absent')

# Replay actual owning System.cpp flags for unselected new reader object only.
# This does not claim the still-Windows System.cpp compiles or that the reader ran.
for mode in ['release', 'debug']:
    database = art.parent / ('macos-runtime-policy-' + mode) / 'compile_commands.json'
    rows = json.loads(database.read_text())
    entry = next(row for row in rows if row['file'].endswith('/vgui2/src/System.cpp'))
    original = shlex.split(entry['command'])
    translated = []
    skip = False
    for token in original:
        if skip:
            skip = False
            continue
        if token == '-o':
            skip = True
        elif token not in ['-c', entry['file']]:
            translated.append(token)
    command = translated + ['-MD', '-MF', str(out / (mode + '-reader.d')), '-c',
        str(source / 'NativeSystemClock.cpp'), '-o', str(out / (mode + '-reader.o'))]
    run(command, mode + '-actual-profile-reader-compile', Path(entry['directory']))
    run(['/usr/bin/nm', '-u', out / (mode + '-reader.o')], mode + '-reader-undefined-symbols')
    # Strict C++14 reader compile proves the helper did not require a global standard upgrade.
    command = [compiler, '-std=c++14', '-Wall', '-Wextra', '-Werror', '-Wconversion', '-pedantic',
        '-ffp-model=precise', '-ffp-contract=off', '-Wno-overriding-option', '-O2' if mode == 'release' else '-O0',
        '-c', source / 'NativeSystemClock.cpp', '-o', out / (mode + '-strict-reader.o')]
    run(command, mode + '-strict-cxx14-reader-compile')
    (out / (mode + '-owning-context.json')).write_text(json.dumps({'original': entry,
        'database': str(database), 'database_sha256': hashlib.sha256(database.read_bytes()).hexdigest(),
        'translation': 'Replace only source and output; add dependency receipt. Reader object not linked or executed.'}, indent=2) + '\n')

version = run([compiler, '--version'], 'compiler-version').stdout.decode()
(out / 'compiler.json').write_text(json.dumps({'path': str(compiler), 'resolved': str(compiler.resolve()),
    'sha256': hashlib.sha256(compiler.resolve().read_bytes()).hexdigest(), 'version': version}, indent=2) + '\n')
print('PASS 3 pure execution profiles; 2 actual owning-profile +2 strict C++14 reader object compiles only')
