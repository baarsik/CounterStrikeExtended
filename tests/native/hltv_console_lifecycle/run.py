#!/usr/bin/env python3
"""Qualify complete production HLTV Console in private stages; never launch a shared runtime."""
import argparse
import hashlib
import json
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
    for path in stage.iterdir():
        if path.is_file():
            print('SHA256', digest(path), path, flush=True)
    failures = []
    for command in ('+quit', '+exit'):
        try:
            check_cleanup(run(stage, 'hltv', ['-ip', '127.0.0.1', '-port', '0', '+modules', command]))
        except AssertionError as error:
            failures.append(f'{command}: {error}')
    try:
        check_cleanup(run(stage, 'hltv', ['-ip', '127.0.0.1', '-port', '0']))
    except AssertionError as error:
        failures.append(f'EOF: {error}')
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
    commands = json.loads((build / 'compile_commands.json').read_text())
    entry = next(item for item in commands if item['file'].endswith('/HLTV/Console/System.cpp'))
    compile_args = shlex.split(entry['command'])
    source = Path(__file__).resolve().with_name('console_paths.cpp')
    obj = stage / 'console_paths.o'
    compile_args[compile_args.index('-c') + 1] = str(source)
    compile_args[compile_args.index('-o') + 1] = str(obj)
    sanitizer_flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer'] if options.sanitize_console else []
    compile_fixture(compile_args + sanitizer_flags, build)
    link = subprocess.check_output(['ninja', '-C', str(build), '-t', 'commands', 'hltv'], text=True).splitlines()[-1]
    link_args = shlex.split(link.split('&&')[1])
    system_object = next(item for item in link_args if item.endswith('/Console/System.cpp.o'))
    link_args[link_args.index(system_object)] = str(obj)
    link_args[link_args.index('-o') + 1] = str(stage / 'console_paths')
    compile_fixture(link_args + sanitizer_flags, build)
    for case in ('repeat', 'deferred'):
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
    print('PASS 9 complete production Console lifecycle cases; all seven module teardowns exactly once per run')


if __name__ == '__main__':
    main()
