"""Run owned CRT/sparse-file probes in a fresh physical workspace, never a runtime producer."""
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
args = parser.parse_args()
args.receipts.mkdir(parents=True, exist_ok=True)
minimum_free = 16 * 1024**3
free = shutil.disk_usage('C:\\').free
(args.receipts / 'host.json').write_text(json.dumps({'platform': platform.platform(), 'machine': platform.machine(),
    'free_bytes_before_copy': free, 'minimum_free_bytes': minimum_free}, indent=2) + '\n')
if free < minimum_free:
    (args.receipts / 'exit.txt').write_text('77\n')
    raise SystemExit(77)
source = args.workspace / 'source'
assert not source.exists(), source
shutil.copytree(args.inputs / 'source', source)
manifest = json.loads((args.inputs / 'source-hashes.json').read_text())
for name, expected in manifest.items():
    assert hashlib.sha256((source / name).read_bytes()).hexdigest() == expected, name
(args.receipts / 'producing-source-hashes.json').write_text(json.dumps(manifest, indent=2) + '\n')
records = []
vs = r'C:\BuildTools\2026\Common7\Tools\VsDevCmd.bat'
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
        data = args.workspace / 'data' / (arch + '-' + config)
        data.mkdir(parents=True)
        row = {'architecture': arch, 'configuration': config, 'phases': []}
        commands = [('configure', ['cmake', '-S', str(source / 'tests/native/filesystem_platform'), '-B', str(build),
                     '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=' + config]),
                    ('build', ['cmake', '--build', str(build), '--parallel', '1']),
                    ('probe', [str(build / 'filesystem_reference.exe'), str(data)])]
        for phase, command in commands:
            with (output / (phase + '.log')).open('wb') as log:
                result = subprocess.run(command, env=environment, stdout=log, stderr=subprocess.STDOUT,
                                        cwd=output, timeout=60, check=False)
            row['phases'].append({'phase': phase, 'exit_code': result.returncode})
            if result.returncode:
                break
        if len(row['phases']) == 3 and row['phases'][-1]['exit_code'] == 0:
            binary = build / 'filesystem_reference.exe'
            contents = binary.read_bytes()
            offset = struct.unpack_from('<I', contents, 0x3c)[0]
            observed = struct.unpack_from('<H', contents, offset + 4)[0]
            assert observed == machine
            row['binary'] = {'sha256': hashlib.sha256(contents).hexdigest(), 'machine': hex(observed)}
            shutil.copy2(binary, output / binary.name)
            shutil.copy2(build / 'compile_commands.json', output / 'compile_commands.json')
            shutil.copy2(build / 'actual_low_level.h', output / 'actual_low_level.h')
        assert not any(data.iterdir()), 'Owned probe temporary files must be removed'
        records.append(row)
        (args.receipts / 'cases.json').write_text(json.dumps(records, indent=2) + '\n')
        print(arch, config, row['phases'], flush=True)
failed = any(len(row['phases']) != 3 or row['phases'][-1]['exit_code'] != 0 for row in records)
(args.receipts / 'exit.txt').write_text(str(int(failed)) + '\n')
raise SystemExit(int(failed))
