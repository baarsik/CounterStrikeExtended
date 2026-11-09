"""Owned console CPU fixture only; compiler environment remains in memory."""
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
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
minimum_free = 4 * 1024 ** 3
args.output.mkdir(parents=True, exist_ok=True)
free = shutil.disk_usage(args.workspace.parent).free
(args.output / 'initial-disk.json').write_text(json.dumps({'free_bytes': free, 'minimum': minimum_free}))
if free < minimum_free:
    raise SystemExit(77)
source = args.workspace / 'source'
assert not source.exists(), source
hashes = json.loads((args.inputs / 'source-hashes.json').read_text())
for name, expected in hashes.items():
    assert hashlib.sha256((args.inputs / 'source' / name).read_bytes()).hexdigest() == expected, name
shutil.copytree(args.inputs / 'source', source)
(args.output / 'source-hashes.json').write_text(json.dumps(hashes, indent=2))
(args.output / 'host.json').write_text(json.dumps({'platform': platform.platform(), 'machine': platform.machine()}))
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
    compiler_path = next(value for key, value in environment.items() if key.upper() == 'PATH')
    compiler = Path(shutil.which('cl.exe', path=compiler_path))
    dumpbin = shutil.which('dumpbin.exe', path=compiler_path)
    compiler_hashes = {str(path): hashlib.sha256(path.read_bytes()).hexdigest()
                      for path in [compiler, compiler.parent / 'c1xx.dll', compiler.parent / 'c2.dll']}
    for config in ['Release', 'Debug']:
        output = args.output / (arch + '-' + config)
        output.mkdir()
        free = shutil.disk_usage(args.workspace.parent).free
        if free < minimum_free:
            (output / 'disk-guard.json').write_text(json.dumps({'free': free, 'minimum': minimum_free}))
            raise SystemExit(77)
        build = args.workspace / (arch + '-' + config)
        row = {'architecture': arch, 'configuration': config, 'free_before': free,
               'compiler_hashes': compiler_hashes, 'phases': []}
        commands = [('configure', ['cmake', '-S', str(source / 'tests/native/cs_smoke_contract'), '-B', str(build),
                                  '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=' + config]),
                    ('build', ['cmake', '--build', str(build), '--parallel', '1']),
                    ('valid', [str(build / 'smoke_cases.exe'), '--valid'])]
        for phase, command in commands:
            with (output / (phase + '.log')).open('wb') as log:
                result = subprocess.run(command, env=environment, stdout=log, stderr=subprocess.STDOUT,
                                        timeout=120, check=False)
            row['phases'].append({'phase': phase, 'command': command, 'exit': result.returncode})
            if result.returncode:
                break
        if len(row['phases']) == 3 and row['phases'][-1]['exit'] == 0:
            image = build / 'smoke_cases.exe'
            raw = image.read_bytes()
            pe = struct.unpack_from('<I', raw, 0x3c)[0]
            assert struct.unpack_from('<H', raw, pe + 4)[0] == machine
            row['image'] = {'sha256': hashlib.sha256(raw).hexdigest(), 'pe_machine': hex(machine)}
            shutil.copy2(image, output / image.name)
            shutil.copy2(build / 'compile_commands.json', output / 'compile_commands.json')
            shutil.copytree(build / 'bodies', output / 'bodies')
            with (output / 'imports.log').open('wb') as log:
                subprocess.run([dumpbin, '/imports', str(image)], env=environment, stdout=log,
                               stderr=subprocess.STDOUT, timeout=30, check=True)
        assert all(hashlib.sha256((source / name).read_bytes()).hexdigest() == value for name, value in hashes.items())
        assert all(hashlib.sha256(Path(name).read_bytes()).hexdigest() == value
                   for name, value in compiler_hashes.items())
        row['free_after'] = shutil.disk_usage(args.workspace.parent).free
        records.append(row)
        (args.output / 'cases.json').write_text(json.dumps(records, indent=2))
        print(arch, config, [phase['exit'] for phase in row['phases']], flush=True)
failed = any(len(row['phases']) != 3 or row['phases'][-1]['exit'] for row in records)
(args.output / 'exit.txt').write_text(str(int(failed)))
raise SystemExit(int(failed))
