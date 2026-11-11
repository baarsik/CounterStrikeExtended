#!/usr/bin/env python3
"""Instrument actual owning callback paths without replacing the retained libraries or controls."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess

FLAGS = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
SOURCES = ['SurfaceSDL.cpp', 'VPanel.cpp', 'StandaloneWindow.cpp', 'StandaloneRaster.cpp']
CASES = ['callback_resize_retirement', 'callback_scheme_retirement', 'callback_think_retirement']


def run(command, cwd, log):
    result = subprocess.run(command, cwd=cwd, capture_output=True)
    log.write_bytes(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(f'Command failed ({result.returncode}): {log}')


def compile_owner(row, destination, log):
    command = shlex.split(row['command'])
    original = command[command.index('-o') + 1]
    clean = []
    skip = False
    for arg in command:
        if skip:
            skip = False
        elif arg in ['-o', '-MF', '-MT', '-MQ']:
            skip = True
        elif arg not in ['-MD', '-MMD']:
            clean.append(arg)
    command = clean + FLAGS + ['-o', str(destination)]
    run(command, row['directory'], log)
    return original, command


def link_command(build, target):
    lines = subprocess.check_output(['ninja', '-C', str(build), '-t', 'commands', target]).decode().splitlines()
    command = shlex.split(lines[-1])
    if command[:2] != [':', '&&'] or command[-2:] != ['&&', ':']:
        raise RuntimeError('Unrecognized owning Ninja link command')
    return command[2:-2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--owner', type=Path, required=True)
    parser.add_argument('--fixture', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--resources', type=Path, required=True)
    parser.add_argument('--config', required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    runtime = args.output / 'runtime'
    if not runtime.exists():
        subprocess.run(['cp', '-cR', str(args.owner / 'runtime' / args.config), str(runtime)], check=True)
    records = []
    replacements = {}
    database = json.loads((args.owner / 'compile_commands.json').read_text())
    for source in SOURCES:
        rows = [row for row in database if Path(row['file']).name == source and 'targets/vgui2/' in row['command']]
        if len(rows) != 1:
            raise RuntimeError(f'Actual producer recipe not unique: {source}')
        row = rows[0]
        output = args.output / (source + '.o')
        original, command = compile_owner(row, output, args.output / (source + '.log'))
        replacements[original] = str(output)
        records.append({'source': row['file'], 'sha256': hashlib.sha256(Path(row['file']).read_bytes()).hexdigest(),
                        'owning_recipe': row, 'sanitized_command': command})
    command = link_command(args.owner, 'runtime/' + args.config + '/vgui2.dylib')
    command = [replacements.get(arg, arg) for arg in command]
    command[command.index('-o') + 1] = str(runtime / 'vgui2.dylib')
    run(command + FLAGS, args.owner, args.output / 'link-vgui.log')
    database = json.loads((args.fixture / 'compile_commands.json').read_text())
    row = next(row for row in database if Path(row['file']).name == 'control_tests.cpp')
    output = args.output / 'control_tests.o'
    original, sanitized = compile_owner(row, output, args.output / 'control_tests.log')
    command = link_command(args.fixture, 'control_tests')
    command = [str(output) if arg == original else arg for arg in command]
    executable = args.output / 'control_tests'
    command[command.index('-o') + 1] = str(executable)
    run(command + FLAGS, args.fixture, args.output / 'link-control.log')
    records.append({'source': row['file'], 'sha256': hashlib.sha256(Path(row['file']).read_bytes()).hexdigest(),
                    'owning_recipe': row, 'sanitized_command': sanitized})
    (args.output / 'recipes.json').write_text(json.dumps(records, indent=2) + '\n')
    environment = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', SDL_RENDER_DRIVER='software',
                       DYLD_LIBRARY_PATH=str(runtime), UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    failures = []
    for case in CASES:
        result = subprocess.run([str(executable), str(runtime), str(args.resources), case], env=environment,
                                capture_output=True)
        log = args.output / (case + '.log')
        log.write_bytes(result.stdout + result.stderr)
        print(case, 'exit', result.returncode, str(log), flush=True)
        if result.returncode:
            failures.append(case)
    if failures:
        raise SystemExit('Actual sanitized callback failures: ' + ', '.join(failures))


if __name__ == '__main__':
    main()
