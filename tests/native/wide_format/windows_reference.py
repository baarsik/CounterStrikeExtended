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
parser.add_argument('--fast-fp', action='store_true')
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
        row = {'architecture': arch, 'configuration': config, 'fast_fp_core': args.fast_fp, 'phases': []}
        commands = [('configure', ['cmake', '-S', str(source / 'tests/native/wide_format'), '-B', str(build),
                                  '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=' + config]),
                    ('reference-build', ['cmake', '--build', str(build), '--parallel', '1', '--target',
                                         'wide_format_reference', 'wide_numeric_reference']),
                    ('probe', [str(build / 'wide_format_reference.exe')]),
                    ('numeric', [str(build / 'wide_numeric_reference.exe')])]
        if args.fast_fp:
            commands[0][1].append('-DCSE_WIDE_FAST_FP=ON')
        if args.helper:
            commands[0][1].append('-DCSE_WIDE_BUILD_HELPER=ON')
            commands.extend([('helper-build', ['cmake', '--build', str(build), '--parallel', '1', '--target',
                                               'wide_format_helper', 'wide_numeric_helper',
                                               'wide_format_edge_tests', 'wide_format_wrapper_tests']),
                             ('helper-wide', [str(build / 'wide_format_helper.exe')]),
                             ('helper-numeric', [str(build / 'wide_numeric_helper.exe')]),
                             ('edge', [str(build / 'wide_format_edge_tests.exe')]),
                             ('wrappers', [str(build / 'wide_format_wrapper_tests.exe')])])
        expected_phases = 9 if args.helper else 4
        for phase, command in commands:
            working = output / phase if phase.startswith('helper-') else output
            working.mkdir(exist_ok=True)
            with (output / (phase + '.log')).open('wb') as log:
                result = subprocess.run(command, env=environment, cwd=working, stdout=log, stderr=subprocess.STDOUT,
                                        timeout=180, check=False)
            row['phases'].append({'phase': phase, 'exit_code': result.returncode})
            if result.returncode:
                break
        if len(row['phases']) == expected_phases and row['phases'][-1]['exit_code'] == 0:
            binary = build / 'wide_format_reference.exe'
            data = binary.read_bytes()
            offset = struct.unpack_from('<I', data, 0x3c)[0]
            observed = struct.unpack_from('<H', data, offset + 4)[0]
            assert observed == machine
            row['binary'] = {'sha256': hashlib.sha256(data).hexdigest(), 'machine': hex(observed)}
            shutil.copy2(binary, output / binary.name)
            numeric = build / 'wide_numeric_reference.exe'
            numeric_data = numeric.read_bytes()
            numeric_offset = struct.unpack_from('<I', numeric_data, 0x3c)[0]
            assert struct.unpack_from('<H', numeric_data, numeric_offset + 4)[0] == machine
            row['numeric_binary'] = {'sha256': hashlib.sha256(numeric_data).hexdigest(), 'machine': hex(machine)}
            shutil.copy2(numeric, output / numeric.name)
            if args.helper:
                for name in ('wide_format_helper.exe', 'wide_numeric_helper.exe', 'wide_format_edge_tests.exe',
                             'wide_format_wrapper_tests.exe'):
                    image = build / name
                    content = image.read_bytes()
                    pe = struct.unpack_from('<I', content, 0x3c)[0]
                    assert struct.unpack_from('<H', content, pe + 4)[0] == machine
                    row[name] = {'sha256': hashlib.sha256(content).hexdigest(), 'machine': hex(machine)}
                    shutil.copy2(image, output / name)
                command = [__import__('sys').executable, str(source / 'tests/native/wide_format/compare_helper.py'),
                    '--reference', str(output), '--helper', str(output / 'helper-wide'),
                    '--numeric-helper', str(output / 'helper-numeric'), '--output', str(output / 'comparison.json')]
                comparison = subprocess.run(command, env=environment, capture_output=True, timeout=60)
                (output / 'comparison.log').write_bytes(comparison.stdout + comparison.stderr)
                row['comparison_exit_code'] = comparison.returncode
            shutil.copytree(build / 'numeric-thresholds', output / 'numeric-thresholds')
            shutil.copy2(build / 'compile_commands.json', output / 'compile_commands.json')
        records.append(row)
        (args.receipts / 'cases.json').write_text(json.dumps(records, indent=2) + '\n')
        print(arch, config, row['phases'], flush=True)
failed = any(len(row['phases']) != (9 if args.helper else 4) or row['phases'][-1]['exit_code'] != 0 or row.get('comparison_exit_code', 0) != 0 for row in records)
(args.receipts / 'exit.txt').write_text(str(int(failed)) + '\n')
raise SystemExit(int(failed))
