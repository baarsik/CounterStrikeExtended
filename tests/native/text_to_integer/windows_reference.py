"""Freeze and execute actual compiler CRT probes without exposing compiler environments."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import shutil
import struct
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--inputs', type=Path, required=True)
parser.add_argument('--workspace', type=Path, required=True)
parser.add_argument('--receipts', type=Path, required=True)
parser.add_argument('--helper', action='store_true')
args = parser.parse_args()
args.receipts.mkdir(parents=True, exist_ok=True)
source = args.workspace / 'source'
assert not source.exists(), source
shutil.copytree(args.inputs / 'source', source)
manifest = json.loads((args.inputs / 'source-hashes.json').read_text())
for name, expected in manifest.items():
    assert hashlib.sha256((source / name).read_bytes()).hexdigest() == expected, name
(args.receipts / 'producing-source-hashes.json').write_text(json.dumps(manifest, indent=2) + '\n')
(args.receipts / 'host.json').write_text(json.dumps({'platform': platform.platform(), 'machine': platform.machine()}, indent=2) + '\n')
vs = r'C:\BuildTools\2026\Common7\Tools\VsDevCmd.bat'
records = []
for arch, machine in [('x86', 0x14c), ('x64', 0x8664), ('arm64', 0xaa64)]:
    initialized = subprocess.run(['cmd.exe', '/d', '/u', '/c',
        f'call {vs} -no_logo -host_arch=arm64 -arch={arch} && set'], capture_output=True, check=True)
    environment = {}
    for line in initialized.stdout.decode('utf-16-le').splitlines():
        key, separator, value = line.partition('=')
        if key and separator:
            environment[key] = value
    assert environment.get('VSCMD_ARG_TGT_ARCH') == arch
    for config in ('Release', 'Debug'):
        build = args.workspace / (arch + '-' + config)
        output = args.receipts / (arch + '-' + config)
        output.mkdir()
        row = {'architecture': arch, 'configuration': config, 'phases': []}
        commands = [('configure', ['cmake', '-S', str(source / 'tests/native/text_to_integer'), '-B', str(build),
                                  '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=' + config]),
                    ('build', ['cmake', '--build', str(build), '--parallel', '1']),
                    ('probe', [str(build / 'text_reference.exe')])]
        if args.helper:
            commands[0][1].extend(['-DCSE_TEXT_BUILD_HELPER=ON', '-DCSE_TEXT_REFERENCE=' + str(output)])
            commands.append(('test', ['ctest', '--test-dir', str(build), '-V', '--output-on-failure']))
        expected_phases = 4 if args.helper else 3
        for phase, command in commands:
            with (output / (phase + '.log')).open('wb') as log:
                result = subprocess.run(command, env=environment, cwd=output, stdout=log, stderr=subprocess.STDOUT,
                                        timeout=180, check=False)
            row['phases'].append({'phase': phase, 'exit_code': result.returncode})
            if result.returncode:
                break
        if len(row['phases']) == expected_phases and row['phases'][-1]['exit_code'] == 0:
            binary = build / 'text_reference.exe'
            data = binary.read_bytes()
            offset = struct.unpack_from('<I', data, 0x3c)[0]
            observed = struct.unpack_from('<H', data, offset + 4)[0]
            assert observed == machine
            row['binary'] = {'sha256': hashlib.sha256(data).hexdigest(), 'machine': hex(observed)}
            shutil.copy2(binary, output / binary.name)
            if args.helper:
                helper_binary = build / 'text_conversion_tests.exe'
                helper_data = helper_binary.read_bytes()
                helper_offset = struct.unpack_from('<I', helper_data, 0x3c)[0]
                assert struct.unpack_from('<H', helper_data, helper_offset + 4)[0] == machine
                row['helper_binary'] = {'sha256': hashlib.sha256(helper_data).hexdigest(), 'machine': hex(machine)}
                shutil.copy2(helper_binary, output / helper_binary.name)
            shutil.copy2(build / 'compile_commands.json', output / 'compile_commands.json')
            shutil.copytree(build / 'corpus', output / 'corpus')
        records.append(row)
        (args.receipts / 'cases.json').write_text(json.dumps(records, indent=2) + '\n')
        print(arch, config, row['phases'], flush=True)
failed = any(len(row['phases']) != (4 if args.helper else 3) or row['phases'][-1]['exit_code'] != 0 for row in records)
(args.receipts / 'exit.txt').write_text(str(int(failed)) + '\n')
raise SystemExit(int(failed))
