"""Run only owned-memory GDI; compiler environments remain in RAM."""
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
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
source = args.workspace / 'source'
assert not source.exists()
shutil.copytree(args.inputs / 'source', source)
hashes = json.loads((args.inputs / 'source-hashes.json').read_text())
assert all(hashlib.sha256((source / name).read_bytes()).hexdigest() == digest for name, digest in hashes.items())
rows = []
for arch, machine in [('x86', 0x14c), ('x64', 0x8664), ('arm64', 0xaa64)]:
    result = subprocess.run(['cmd.exe', '/d', '/u', '/c',
        r'call C:\BuildTools\2026\Common7\Tools\VsDevCmd.bat -no_logo -host_arch=arm64 -arch=' + arch + ' && set'],
        capture_output=True, check=True)
    environment = {}
    for line in result.stdout.decode('utf-16-le').splitlines():
        key, separator, value = line.partition('=')
        if key and separator:
            environment[key] = value
    for configuration in ('Release', 'Debug'):
        output = args.output / (arch + '-' + configuration)
        output.mkdir(parents=True)
        build = args.workspace / (arch + '-' + configuration)
        commands = [('configure', ['cmake', '-S', str(source / 'tests/native/vgui1_legacy_raster'),
            '-B', str(build), '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=' + configuration,
            '-DCSE_RASTER_REFERENCE_ONLY=ON']), ('build', ['cmake', '--build', str(build), '--parallel', '1']),
            ('gdi', [str(build / 'raster_gdi_reference.exe'), str(build / 'owned-inputs/owned-raster.ttf')])]
        row = {'architecture': arch, 'configuration': configuration, 'phases': []}
        for phase, command in commands:
            with (output / (phase + '.log')).open('wb') as log:
                completed = subprocess.run(command, env=environment, cwd=output,
                    stdout=log, stderr=subprocess.STDOUT, timeout=90)
            row['phases'].append({'phase': phase, 'command': command, 'exit': completed.returncode})
            if completed.returncode:
                break
        if len(row['phases']) == 3 and row['phases'][-1]['exit'] == 0:
            image = build / 'raster_gdi_reference.exe'
            data = image.read_bytes()
            pe = struct.unpack_from('<I', data, 0x3c)[0]
            assert struct.unpack_from('<H', data, pe + 4)[0] == machine
            row['binary_sha256'] = hashlib.sha256(data).hexdigest()
            row['pe_machine'] = hex(machine)
            shutil.copy2(image, output / image.name)
            shutil.copy2(build / 'compile_commands.json', output / 'compile_commands.json')
            shutil.copytree(build / 'extracted', output / 'extracted')
            shutil.copytree(build / 'owned-inputs', output / 'owned-inputs')
        rows.append(row)
        (args.output / 'cases.json').write_text(json.dumps(rows, indent=2) + '\n')
        print(arch, configuration, [row['exit'] for row in row['phases']], flush=True)
raise SystemExit(int(any(len(row['phases']) != 3 or row['phases'][-1]['exit'] for row in rows)))
