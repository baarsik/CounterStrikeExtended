#!/usr/bin/env python3
"""Use frozen Windows NetSocket source with actual classes and fake syscalls; no runtime sockets."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess



def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--arch', choices=['x86', 'x64', 'arm64'], required=True)
    options = parser.parse_args()
    options.output.mkdir(parents=True, exist_ok=True)
    inputs = options.output / 'input'
    shutil.copytree(options.input, inputs, dirs_exist_ok=True)
    sdk = inputs / 'GoldSrc-SDK'
    metadata = json.loads((inputs / 'fixture-input.json').read_text())
    shared = metadata['shared_cases']
    sources = metadata['sources']
    compiler = Path(shutil.which('cl.exe')).resolve()
    receipt = {'compiler': str(compiler), 'compiler_sha256': hashlib.sha256(compiler.read_bytes()).hexdigest(),
               'arch': options.arch, 'compiles': [], 'cases': [], 'whole_tus': []}
    flags = ['/nologo', '/W4', '/WX', '/EHsc', '/GR', '/fp:precise', '/DWIN32', '/D_WINDOWS', '/D_USRDLL',
             '/DNOMINMAX', '/D_CRT_SECURE_NO_WARNINGS', '/D_WINSOCK_DEPRECATED_NO_WARNINGS']
    flags += ['/I' + str(sdk / name) for name in ['HLTV/Core/private', 'HLTV/Core/private/public',
              'HLTV/Core/private/common', 'HLTV/common', 'launcher/public', '.']] + ['/I' + str(inputs)]
    for config, extra in [('release', ['/O2', '/MD', '/DNDEBUG']), ('debug', ['/Od', '/RTC1', '/Zi', '/MDd', '/D_DEBUG'])]:
        build = options.output / config
        build.mkdir(exist_ok=True)
        objects = []
        for index, name in enumerate(sources + ['fixture']):
            source = inputs / 'socket_tests.cpp' if name == 'fixture' else sdk / name
            if name == 'HLTV/Core/NetSocket.cpp':
                source = inputs / 'NetSocket-original.cpp'
            hooks = ['/FI' + str(inputs / 'syscalls.h')] if name in ['HLTV/Core/NetSocket.cpp', 'HLTV/Core/Network.cpp'] else []
            obj = build / (str(index) + '.obj')
            command = [str(compiler), *flags, '/std:c++14', *extra, *hooks, '/c', str(source), '/Fo' + str(obj)]
            result = subprocess.run(command, cwd=build, capture_output=True, timeout=120)
            (build / (str(index) + '-compile.log')).write_bytes(result.stdout + result.stderr)
            receipt['compiles'].append({'config': config, 'source': str(source), 'args': command,
                'exit': result.returncode, 'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest()})
            if result.returncode:
                (options.output / 'receipt.json').write_text(json.dumps(receipt, indent=2))
                raise RuntimeError('actual-source component compilation failed: ' + str(build / (str(index) + '-compile.log')))
            objects.append(str(obj))
        binary = build / 'socket-tests.exe'
        command = [str(compiler), '/nologo', *extra, *objects, '/Fe:' + str(binary), '/link', '/WX', 'ws2_32.lib']
        result = subprocess.run(command, cwd=build, capture_output=True, timeout=120)
        (build / 'link.log').write_bytes(result.stdout + result.stderr)
        assert result.returncode == 0, (build / 'link.log').read_text()
        for index, case in enumerate(shared):
            run = subprocess.run([str(binary), case], capture_output=True, timeout=10)
            (build / (str(index) + '.log')).write_bytes(run.stdout + run.stderr)
            receipt['cases'].append({'config': config, 'case': case, 'exit': run.returncode})
        source = sdk / 'HLTV/Core/NetSocket.cpp'
        command = [str(compiler), *flags, '/std:c++17', *extra, '/c', str(source), '/Fo' + str(build / 'current-whole.obj')]
        result = subprocess.run(command, cwd=build, capture_output=True, timeout=120)
        (build / 'whole-compile.log').write_bytes(result.stdout + result.stderr)
        receipt['whole_tus'].append({'config': config, 'args': command, 'exit': result.returncode,
            'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest()})
    (options.output / 'receipt.json').write_text(json.dumps(receipt, indent=2))
    assert all(row['exit'] == 0 for row in receipt['cases'] + receipt['whole_tus'])
    print('PASS', options.arch, len(receipt['cases']), 'frozen-reference cases and', len(receipt['whole_tus']), 'current whole TUs')


if __name__ == '__main__':
    main()
