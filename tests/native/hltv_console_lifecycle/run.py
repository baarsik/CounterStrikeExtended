#!/usr/bin/env python3
"""Qualify complete production HLTV Console in private stages; never launch a shared runtime."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import signal
import subprocess
import tempfile
import time


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def check_cleanup(output, expected=1, initialized=True):
    for module in ('Proxy', 'Network', 'Server', 'World', 'Master', 'Status', 'Demo'):
        if initialized and module in ('Proxy', 'Network', 'Server', 'World'):
            require(re.search(rf'{module}(?: module)? initialized\.', output), f'{module} did not initialize')
        matches = re.findall(rf'{module}(?: module)? shut ?down\.', output, re.IGNORECASE)
        require(len(matches) == expected, f'{module} cleanup count {len(matches)}, expected {expected}')


def run(stage, name, arguments, expected=0):
    result = subprocess.run([str(stage / name), *arguments], cwd=stage, input=b'',
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=20)
    output = result.stdout.decode(errors='replace')
    (stage / ('-'.join(arguments or ['signal']) + '.log')).write_text(output)
    print(output)
    require(result.returncode == expected, f'{name} returned {result.returncode}, expected {expected}')
    return output


def compile_fixture(arguments, build):
    print('BUILD', shlex.join(arguments), flush=True)
    result = subprocess.run(arguments, cwd=build, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if result.returncode:
        print(result.stdout.decode(errors='replace'), flush=True)
    result.check_returncode()


def run_without_input(stage, mode):
    """EOF retires console input, while an explicit signal owns process shutdown."""
    log_name = 'input-' + mode
    arguments = [str(stage / 'hltv'), '-ip', '127.0.0.1', '-port', '0', '+logfile', log_name]
    source = subprocess.PIPE if mode == 'pipe-eof' else subprocess.DEVNULL
    with (stage / (log_name + '.log')).open('wb') as log:
        process = subprocess.Popen(arguments, cwd=stage, stdin=source, stdout=log, stderr=subprocess.STDOUT,
                                   preexec_fn=(lambda: os.close(0)) if mode == 'closed-stdin' else None)
        try:
            deadline = time.monotonic() + 15
            while "Type 'help'" not in (stage / (log_name + '.log')).read_text():
                require(process.poll() is None, f'{mode}: process exited before readiness')
                require(time.monotonic() < deadline, f'{mode}: process did not become ready')
                time.sleep(0.02)
            if mode == 'pipe-eof':
                process.stdin.close()
            # Span several console polls, then require the broadcaster to remain alive.
            time.sleep(0.35)
            require(process.poll() is None, f'{mode}: stdin ending stopped the process')
            process.send_signal(signal.SIGTERM)
            require(process.wait(timeout=15) == 0, f'{mode}: explicit shutdown failed')
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
            if process.stdin is not None and not process.stdin.closed:
                process.stdin.close()
    check_cleanup((stage / 'valve' / (log_name + '.txt')).read_text(), initialized=False)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--runtime-build', type=Path, required=True)
    parser.add_argument('--filesystem', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--sanitize-console', action='store_true')
    options = parser.parse_args()
    build = options.runtime_build.resolve()
    options.output.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix='console-', dir=options.output.resolve()))
    print('STAGE', stage, flush=True)
    (stage / 'valve').mkdir()
    profile = re.search(r'CMAKE_BUILD_TYPE:STRING=(.*)', (build / 'CMakeCache.txt').read_text())[1]
    runtime = build / 'runtime' / profile
    for name in ('hltv', 'Core.dylib', 'proxy.dylib', 'libsteam_api.dylib'):
        shutil.copy2(runtime / name, stage / name)
    shutil.copy2(options.filesystem, stage / 'filesystem_stdio.dylib')
    commands = json.loads((build / 'compile_commands.json').read_text())
    entry = next(item for item in commands if item['file'].endswith('/HLTV/Console/System.cpp'))
    root = Path(__file__).resolve().parents[3]
    source_prefix = entry['file'].split('/GoldSrc-SDK/')[0]
    compile_args = [value.replace(source_prefix, str(root)) for value in shlex.split(entry['command'])]
    sanitizer_flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if options.sanitize_console else []
    link = subprocess.check_output(['ninja', '-C', str(build), '-t', 'commands', 'hltv'], text=True).splitlines()[-1]
    link_args = shlex.split(link.split('&&')[1])
    system_object = next(item for item in link_args if item.endswith('/Console/System.cpp.o'))

    def compile_owner(source, name):
        obj = stage / (name + '.o')
        arguments = compile_args.copy()
        arguments[arguments.index('-c') + 1] = str(source)
        arguments[arguments.index('-o') + 1] = str(obj)
        compile_fixture(arguments + sanitizer_flags, build)
        arguments = link_args.copy()
        arguments[arguments.index(system_object)] = str(obj)
        arguments[arguments.index('-o') + 1] = str(stage / name)
        compile_fixture(arguments + sanitizer_flags, build)

    # Compile both entry points from this checkout, without modifying the configured runtime's objects.
    compile_owner(root / 'GoldSrc-SDK/HLTV/Console/System.cpp', 'hltv')
    compile_owner(Path(__file__).resolve().with_name('console_paths.cpp'), 'console_paths')
    for path in stage.iterdir():
        if path.is_file():
            print('SHA256', digest(path), path, flush=True)
    failures = []
    for command in ('+quit', '+exit'):
        try:
            check_cleanup(run(stage, 'hltv', ['-ip', '127.0.0.1', '-port', '0', '+modules', command]))
        except AssertionError as error:
            failures.append(f'{command}: {error}')
    for mode in ('devnull', 'pipe-eof', 'closed-stdin'):
        try:
            run_without_input(stage, mode)
        except AssertionError as error:
            failures.append(f'{mode}: {error}')
    # Use only the owned child and wait for its actual initialization before sending SIGTERM.
    with (stage / 'sigterm.log').open('wb') as log:
        process = subprocess.Popen([str(stage / 'hltv'), '-ip', '127.0.0.1', '-port', '0', '+logfile', 'sigterm-cleanup'], cwd=stage,
                                   stdin=subprocess.PIPE, stdout=log, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + 15
            while 'Type \'help\'' not in (stage / 'sigterm.log').read_text():
                require(process.poll() is None, 'signal process exited before readiness')
                require(time.monotonic() < deadline, 'signal process did not become ready')
                time.sleep(0.02)
            process.send_signal(signal.SIGTERM)
            require(process.wait(timeout=15) == 0, 'signal process failed')
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
            process.stdin.close()
    try:
        check_cleanup((stage / 'valve' / 'sigterm-cleanup.txt').read_text(), initialized=False)
    except AssertionError as error:
        failures.append(f'SIGTERM: {error}')
    for case in ('repeat', 'deferred', 'eof-continuity'):
        try:
            check_cleanup(run(stage, 'console_paths', [case]), expected=2)
        except AssertionError as error:
            failures.append(f'{case}: {error}')
    try:
        run(stage, 'console_paths', ['preconsole'])
    except AssertionError as error:
        failures.append(f'preconsole: {error}')
    (stage / 'proxy.dylib').rename(stage / 'proxy.hidden')
    try:
        run(stage, 'console_paths', ['partial'], expected=0)
    except AssertionError as error:
        failures.append(f'partial: {error}')
    (stage / 'filesystem_stdio.dylib').rename(stage / 'filesystem.hidden')
    try:
        run(stage, 'console_paths', ['early'], expected=0)
    except AssertionError as error:
        failures.append(f'early: {error}')
    print('FAILURES', failures, flush=True)
    require(not failures, '; '.join(failures))
    print('PASS 12 complete production Console lifecycle cases; all seven module teardowns exactly once per run')


if __name__ == '__main__':
    main()
