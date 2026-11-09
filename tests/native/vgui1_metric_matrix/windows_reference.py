"""Only diagnostic metric matrix on matched transient memory, with no font-program files."""
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
parser.add_argument('--architectures', nargs='+', choices=['x86', 'x64', 'arm64'], default=['x64'])
args = parser.parse_args()
source = args.workspace / 'source'
assert not source.exists()
shutil.copytree(args.inputs / 'source', source)
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
hashes = json.loads((args.inputs / 'source-hashes.json').read_text())
assert all(sha(source / name) == digest for name, digest in hashes.items())
rows = []
machines = {'x86': 0x14c, 'x64': 0x8664, 'arm64': 0xaa64}
for architecture in args.architectures:
    initialized = subprocess.run(['cmd.exe', '/d', '/u', '/c',
        r'call C:\BuildTools\2026\Common7\Tools\VsDevCmd.bat -no_logo -host_arch=arm64 -arch='
        + architecture + ' && set'], capture_output=True, check=True)
    environment = {}
    for line in initialized.stdout.decode('utf-16-le').splitlines():
        key, separator, value = line.partition('=')
        if key and separator:
            environment[key] = value
    compiler_path = next(value for key, value in environment.items() if key.upper() == 'PATH')
    dumpbin = shutil.which('dumpbin.exe', path=compiler_path)
    assert dumpbin, 'The initialized compiler environment must provide dumpbin'
    prefix = (Path(r'\\Mac\CseNativeSource\GoldSrc-SDK\external\installed\x86-windows-static-md')
        if architecture == 'x86' else Path(r'C:\CI\CSE-Native64-Vcpkg-20261108')
        / ('installed-' + architecture) / (architecture + '-windows-static-md'))
    expected = json.loads((args.inputs / 'pinned-windows-freetype.json').read_text())[architecture]
    assert all(sha(Path(path)) == digest for path, digest in expected.items())
    for configuration in ['Release', 'Debug']:
        output = args.output / (architecture + '-' + configuration)
        output.mkdir(parents=True)
        build = args.workspace / (architecture + '-' + configuration)
        row = {'architecture': architecture, 'configuration': configuration, 'prefix': str(prefix),
               'freetype-before': {path: sha(Path(path)) for path in expected}, 'phases': []}
        commands = [('configure', ['cmake', '-S', str(source / 'tests/native/vgui1_metric_matrix'),
            '-B', str(build), '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=' + configuration,
            '-DCSE_FREETYPE_PREFIX=' + str(prefix)]),
            ('build', ['cmake', '--build', str(build), '--target', 'metric_matrix', '--parallel', '1']),
            ('metric-matrix', [str(build / 'metric_matrix.exe')])]
        for phase, command in commands:
            with (output / (phase + '.log')).open('wb') as log:
                completed = subprocess.run(command, env=environment, cwd=output,
                    stdout=log, stderr=subprocess.STDOUT, timeout=90)
            row['phases'].append({'phase': phase, 'command': command, 'exit': completed.returncode})
            if completed.returncode:
                break
        if len(row['phases']) >= 2 and row['phases'][1]['exit'] == 0:
            image = build / 'metric_matrix.exe'
            data = image.read_bytes()
            pe = struct.unpack_from('<I', data, 0x3c)[0]
            assert struct.unpack_from('<H', data, pe + 4)[0] == machines[architecture]
            row['binary-sha256'] = sha(image)
            row['pe-machine'] = hex(machines[architecture])
            shutil.copy2(image, output / image.name)
            shutil.copy2(build / 'compile_commands.json', output / 'compile_commands.json')
            shutil.copytree(build / 'extracted', output / 'extracted')
            shutil.copytree(build / 'control/extracted', output / 'gdi-extracted')
            with (output / 'imports.log').open('wb') as log:
                subprocess.run([dumpbin, '/imports', str(image)], env=environment, stdout=log,
                    stderr=subprocess.STDOUT, timeout=15, check=True)
        row['freetype-after'] = {path: sha(Path(path)) for path in expected}
        assert row['freetype-before'] == row['freetype-after'] == expected
        assert all(sha(source / name) == digest for name, digest in hashes.items())
        row['allowed-output-hashes'] = {p.name: sha(p) for p in sorted(output.iterdir()) if p.is_file()}
        assert not any(p.suffix.lower() in ['.ttf', '.otf', '.ttc', '.fon', '.dmp'] for p in output.iterdir())
        rows.append(row)
        (args.output / 'cases.json').write_text(json.dumps(rows, indent=2) + '\n')
        print(architecture, configuration, [x['exit'] for x in row['phases']], flush=True)
raise SystemExit(int(any(len(x['phases']) != 3 or x['phases'][-1]['exit'] for x in rows)))
