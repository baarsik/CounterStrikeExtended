"""Run only controlled actual-method fixtures; compiler environments stay in process memory."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--inputs', type=Path, required=True)
parser.add_argument('--workspace', type=Path, required=True)
parser.add_argument('--receipts', type=Path, required=True)
args = parser.parse_args()
args.receipts.mkdir(parents=True, exist_ok=True)
manifest = json.loads((args.inputs / 'source-hashes.json').read_text())
source = args.workspace / 'source'
assert not source.exists(), source
shutil.copytree(args.inputs / 'source', source)
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
        row = {'architecture': arch, 'configuration': config, 'phases': [], 'binaries': {}}
        configure = ['cmake', '-S', str(source / 'tests/native/network_platform'), '-B', str(build), '-G', 'Ninja',
                     '-DCMAKE_BUILD_TYPE=' + config, '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON']
        for phase, command in [('configure', configure), ('build', ['cmake', '--build', str(build), '--parallel', '1']),
                               ('test', ['ctest', '--test-dir', str(build), '-V', '--output-on-failure'])]:
            log = args.receipts / (arch + '-' + config + '-' + phase + '.log')
            with log.open('wb') as output:
                completed = subprocess.run(command, env=environment, stdout=output, stderr=subprocess.STDOUT,
                                           timeout=180, check=False)
            row['phases'].append({'phase': phase, 'exit_code': completed.returncode})
            if completed.returncode:
                break
        if len(row['phases']) == 3 and row['phases'][-1]['exit_code'] == 0:
            for binary in sorted(build.glob('*.exe')):
                data = binary.read_bytes()
                offset = struct.unpack_from('<I', data, 0x3c)[0]
                assert data[offset:offset + 4] == b'PE\0\0'
                observed = struct.unpack_from('<H', data, offset + 4)[0]
                assert observed == machine, (binary, hex(observed), hex(machine))
                destination = args.receipts / (arch + '-' + config) / binary.name
                destination.parent.mkdir(exist_ok=True)
                shutil.copy2(binary, destination)
                row['binaries'][binary.name] = {'sha256': hashlib.sha256(data).hexdigest(), 'machine': hex(observed)}
            for policy in ('CSE_ENGINE_WSOCK32', 'CSE_ENGINE_WINSOCK2'):
                packet_dir = build / ('wire_' + policy)
                destination = args.receipts / (arch + '-' + config) / ('wire_' + policy)
                shutil.copytree(packet_dir, destination)
            shutil.copy2(build / 'compile_commands.json', args.receipts / (arch + '-' + config + '-commands.json'))
        records.append(row)
        (args.receipts / 'cases.json').write_text(json.dumps(records, indent=2) + '\n')
        print(arch, config, row['phases'], flush=True)
failed = any(len(row['phases']) != 3 or row['phases'][-1]['exit_code'] != 0 for row in records)
(args.receipts / 'exit.txt').write_text(str(int(failed)) + '\n')
raise SystemExit(int(failed))
