"""Build the new TU and execute only the independent pure-transfer gates on Windows."""
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
parser.add_argument('--reference', type=Path, required=True)
args = parser.parse_args()
source = args.workspace / 'source'
assert not source.exists()
shutil.copytree(args.inputs / 'source', source)
hashes = json.loads((args.inputs / 'source-hashes.json').read_text())
assert all(hashlib.sha256((source / name).read_bytes()).hexdigest() == digest for name, digest in hashes.items())
rows = []
for architecture, machine in [('x86', 0x14c), ('x64', 0x8664), ('arm64', 0xaa64)]:
    initialized = subprocess.run(['cmd.exe', '/d', '/u', '/c',
        r'call C:\BuildTools\2026\Common7\Tools\VsDevCmd.bat -no_logo -host_arch=arm64 -arch=' + architecture + ' && set'],
        capture_output=True, check=True)
    environment = {}
    for line in initialized.stdout.decode('utf-16-le').splitlines():
        key, separator, value = line.partition('=')
        if key and separator:
            environment[key] = value
    prefix = (Path(r'\\Mac\CseNativeSource\GoldSrc-SDK\external\installed\x86-windows-static-md')
        if architecture == 'x86' else Path(r'C:\CI\CSE-Native64-Vcpkg-20261108')
        / ('installed-' + architecture) / (architecture + '-windows-static-md'))
    for configuration in ('Release', 'Debug'):
        output = args.output / (architecture + '-' + configuration)
        output.mkdir(parents=True)
        build = args.workspace / (architecture + '-' + configuration)
        reference = args.reference / (architecture + '-' + configuration)
        commands = [('configure', ['cmake', '-S', str(source / 'tests/native/vgui1_legacy_raster'),
            '-B', str(build), '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=' + configuration,
            '-DCSE_FREETYPE_PREFIX=' + str(prefix)]),
            ('build', ['cmake', '--build', str(build), '--target', 'raster_pixel_tests', 'raster_replay_tests',
                '--parallel', '1']), ('bounds', [str(build / 'raster_pixel_tests.exe')]),
            ('rgb', [str(build / 'raster_pixel_tests.exe'), 'rgb']),
            ('replay', [str(build / 'raster_replay_tests.exe'), str(reference)])]
        row = {'architecture': architecture, 'configuration': configuration, 'prefix': str(prefix), 'phases': []}
        for phase, command in commands:
            with (output / (phase + '.log')).open('wb') as log:
                result = subprocess.run(command, env=environment, cwd=build if build.exists() else output,
                    stdout=log, stderr=subprocess.STDOUT, timeout=90)
            row['phases'].append({'phase': phase, 'command': command, 'exit': result.returncode})
            if result.returncode:
                break
        if len(row['phases']) == 5 and row['phases'][-1]['exit'] == 0:
            for name in ('raster_pixel_tests.exe', 'raster_replay_tests.exe'):
                data = (build / name).read_bytes()
                pe = struct.unpack_from('<I', data, 0x3c)[0]
                assert struct.unpack_from('<H', data, pe + 4)[0] == machine
                shutil.copy2(build / name, output / name)
                row[name] = {'sha256': hashlib.sha256(data).hexdigest(), 'pe_machine': hex(machine)}
            shutil.copy2(build / 'compile_commands.json', output / 'compile_commands.json')
            locations = list((prefix / 'lib').glob('*freetype*')) + list((prefix / 'debug/lib').glob('*freetype*'))
            locations += [prefix / 'include/freetype/freetype.h', prefix / 'include/freetype/config/ftoption.h']
            row['freetype_inputs'] = {str(path): hashlib.sha256(path.read_bytes()).hexdigest()
                                      for path in locations if path.is_file()}
        rows.append(row)
        (args.output / 'cases.json').write_text(json.dumps(rows, indent=2) + '\n')
        print(architecture, configuration, [(phase['phase'], phase['exit']) for phase in row['phases']], flush=True)
raise SystemExit(int(any(len(row['phases']) != 5 or row['phases'][-1]['exit'] for row in rows)))
