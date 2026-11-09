"""Exactly one source-matching family/request; fonts remain in VM1 memory only."""
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
args.output.mkdir(parents=True, exist_ok=True)
free = shutil.disk_usage(args.workspace.parent).free
(args.output / 'disk-before.json').write_text(json.dumps({'free': free, 'minimum': 4 * 1024 ** 3}))
if free < 4 * 1024 ** 3:
    raise SystemExit(77)
source = args.workspace / 'source'
assert not source.exists()
hashes = json.loads((args.inputs / 'source-hashes.json').read_text())
for name, expected in hashes.items():
    assert hashlib.sha256((args.inputs / 'source' / name).read_bytes()).hexdigest() == expected
shutil.copytree(args.inputs / 'source', source)
pinned = json.loads((args.inputs / 'pinned-windows-freetype.json').read_text())
records = []
for arch, machine, config in [(arch, machine, config)
        for arch, machine in [('x86', 0x14c), ('x64', 0x8664)]
        for config in ('Release', 'Debug')]:
    if shutil.disk_usage(args.workspace.parent).free < 4 * 1024 ** 3:
        raise SystemExit(77)
    result = subprocess.run(['cmd.exe', '/d', '/u', '/c',
        r'call C:\BuildTools\2026\Common7\Tools\VsDevCmd.bat -no_logo -host_arch=arm64 -arch='
        + arch + ' && set'], capture_output=True, check=True)
    environment = {}
    for line in result.stdout.decode('utf-16-le').splitlines():
        key, separator, value = line.partition('=')
        if key and separator:
            environment[key] = value
    paths = next(value for key, value in environment.items() if key.upper() == 'PATH')
    compiler = Path(shutil.which('cl.exe', path=paths))
    compiler_pins = {str(p): hashlib.sha256(p.read_bytes()).hexdigest()
                     for p in (compiler, compiler.parent / 'c1xx.dll', compiler.parent / 'c2.dll')}
    prefix = (Path(r'\\Mac\CseNativeSource\GoldSrc-SDK\external\installed\x86-windows-static-md')
              if arch == 'x86' else Path(r'C:\CI\CSE-Native64-Vcpkg-20261108\installed-x64\x64-windows-static-md'))
    for name, expected in pinned[arch].items():
        assert hashlib.sha256(Path(name).read_bytes()).hexdigest() == expected, name
    output = args.output / (arch + '-' + config)
    output.mkdir()
    build = args.workspace / (arch + '-' + config)
    row = {'architecture': arch, 'configuration': config, 'phases': [],
           'compiler_hashes': compiler_pins, 'freetype_hashes': pinned[arch]}
    commands = [('configure', ['cmake', '-S', str(source / 'tests/native/vgui1_fixedsys_source_probe'),
        '-B', str(build), '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=' + config, '-DCSE_FREETYPE_PREFIX=' + str(prefix)]),
        ('build', ['cmake', '--build', str(build), '--parallel', '1']),
        ('exact-query', [str(build / 'fixedsys_source_probe.exe')])]
    for phase, command in commands:
        process = subprocess.run(command, env=environment, cwd=output, capture_output=True, timeout=90)
        (output / (phase + '.stdout.log')).write_bytes(process.stdout)
        (output / (phase + '.stderr.log')).write_bytes(process.stderr)
        row['phases'].append({'phase': phase, 'command': command, 'exit': process.returncode})
        if process.returncode:
            break
    if len(row['phases']) == 3 and row['phases'][-1]['exit'] == 0:
        image = build / 'fixedsys_source_probe.exe'
        data = image.read_bytes()
        assert struct.unpack_from('<H', data, struct.unpack_from('<I', data, 0x3c)[0] + 4)[0] == machine
        row['image_sha256'] = hashlib.sha256(data).hexdigest()
        shutil.copy2(image, output / image.name)
        shutil.copy2(build / 'compile_commands.json', output / 'compile_commands.json')
        shutil.copytree(build / 'extracted', output / 'extracted')
    for name, expected in {**pinned[arch], **compiler_pins}.items():
        assert hashlib.sha256(Path(name).read_bytes()).hexdigest() == expected, name
    assert not any(p.suffix.lower() in ('.ttf', '.otf', '.ttc', '.fon', '.fnt', '.png', '.bmp', '.dmp')
                   for p in output.iterdir())
    row['free_after'] = shutil.disk_usage(args.workspace.parent).free
    records.append(row)
    (args.output / 'cases.json').write_text(json.dumps(records, indent=2))
    print(arch, config, [phase['exit'] for phase in row['phases']], flush=True)
    if row['phases'][-1]['exit']:
        break
raise SystemExit(int(any(len(row['phases']) != 3 or row['phases'][-1]['exit'] for row in records)))
