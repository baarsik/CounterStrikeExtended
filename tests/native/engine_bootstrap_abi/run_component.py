#!/usr/bin/env python3
"""Run an in-process descriptor/callback model; never load an owned runtime module."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--header', type=Path, required=True)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--bindings', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--profile', choices=['release', 'debug', 'sanitized'], required=True)
    parser.add_argument('--archive', type=Path)
    options = parser.parse_args()
    options.output.mkdir(parents=True, exist_ok=True)
    include = options.output / 'include/public'
    include.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(options.header, include / 'cse_engine_bootstrap_abi.h')
    actual_fs = (options.root / 'src/public/cse_filesystem_abi.h').as_posix()
    (include / 'cse_filesystem_abi.h').write_text('#pragma once\n#include "' + actual_fs + '"\n')
    if os.name == 'nt':
        assert options.profile != 'sanitized'
        compiler = Path(shutil.which('cl'))
        executable = options.output / 'contract_test.exe'
        command = [str(compiler), '/nologo', '/std:c++14', '/EHsc', '/W4', '/WX', '/fp:precise', '/DWIN32',
                   '/MDd' if options.profile == 'debug' else '/MD',
                   '/Od' if options.profile == 'debug' else '/O2',
                   '/I' + str(include.parent), '/I' + str(options.root / 'src'),
                   '/I' + str(options.root / 'src/public'), '/I' + str(options.bindings),
                   str(options.source), '/Fe:' + str(executable),
                   '/Fo' + str(options.output / 'contract_test.obj'),
                   '/Fd' + str(options.output / 'contract_test.pdb')]
    else:
        compiler = Path('/usr/bin/c++')
        executable = options.output / 'contract_test'
        flags = {'release': ['-O2'], 'debug': ['-O0', '-g'],
                 'sanitized': ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']}
        command = [str(compiler), '-std=c++14', '-DOSX', '-DPOSIX', '-mmacosx-version-min=14.0',
                   '-Wall', '-Wextra', '-Werror', '-ffp-model=precise']
        command += flags[options.profile] + ['-I' + str(include.parent),
            '-I' + str(options.root / 'src'), '-I' + str(options.root / 'src/public'),
            '-I' + str(options.bindings), str(options.source), '-o', str(executable)]
    compile_run = subprocess.run(command, cwd=options.output, capture_output=True, timeout=120)
    (options.output / 'compile.log').write_bytes(compile_run.stdout + compile_run.stderr)
    rows = [{'phase': 'compile', 'args': command, 'exit': compile_run.returncode}]
    if compile_run.returncode == 0:
        run = subprocess.run([str(executable)], cwd=options.output, capture_output=True, timeout=30)
        (options.output / 'run.log').write_bytes(run.stdout + run.stderr)
        rows.append({'phase': 'run', 'args': [str(executable)], 'exit': run.returncode})
        print(run.stdout.decode(errors='replace') + run.stderr.decode(errors='replace'), end='')
    (options.output / 'receipt.json').write_text(json.dumps({
        'rows': rows, 'compiler': str(compiler), 'compiler_sha256': hashlib.sha256(compiler.read_bytes()).hexdigest(),
        'header_sha256': hashlib.sha256(options.header.read_bytes()).hexdigest(),
        'executable_sha256': hashlib.sha256(executable.read_bytes()).hexdigest() if executable.is_file() else None,
        'source_sha256': hashlib.sha256(options.source.read_bytes()).hexdigest(),
        'qualification': 'standalone C++14 descriptor/typed-callback component model; no module load or game/provider execution'},
        indent=2))
    if options.archive:
        options.archive.mkdir(parents=True, exist_ok=True)
        for path in options.output.iterdir():
            if path.is_file() and path.suffix in ['.log', '.json']:
                shutil.copyfile(path, options.archive / path.name)
    if any(row['exit'] for row in rows) or len(rows) != 2:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
