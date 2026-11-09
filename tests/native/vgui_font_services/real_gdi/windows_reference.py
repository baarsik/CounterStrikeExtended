"""Freeze and execute real owned-memory GDI references; no desktop/game execution."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--inputs', type=Path, required=True)
p.add_argument('--workspace', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
source = a.workspace / 'source'
assert not source.exists()
shutil.copytree(a.inputs / 'source', source)
manifest = json.loads((a.inputs / 'source-hashes.json').read_text())
for name, expected in manifest.items():
    assert hashlib.sha256((source / name).read_bytes()).hexdigest() == expected
rows = []
font = r'C:\Program Files (x86)\Steam\steamapps\common\Half-Life\valve\resource\marlett.ttf'
for arch, machine in [('x86', 0x14c), ('x64', 0x8664), ('arm64', 0xaa64)]:
    initialized = subprocess.run(['cmd.exe', '/d', '/u', '/c',
        r'call C:\BuildTools\2026\Common7\Tools\VsDevCmd.bat -no_logo -host_arch=arm64 -arch=' + arch + ' && set'],
        capture_output=True, check=True)
    environment = {}
    for line in initialized.stdout.decode('utf-16-le').splitlines():
        key, separator, value = line.partition('=')
        if key and separator:
            environment[key] = value
    assert environment['VSCMD_ARG_TGT_ARCH'] == arch
    for configuration in ('Release', 'Debug'):
        out = a.output / (arch + '-' + configuration)
        out.mkdir(parents=True)
        build = a.workspace / (arch + '-' + configuration)
        commands = [('configure', ['cmake', '-S', str(source / 'tests/native/vgui_font_services/real_gdi'),
            '-B', str(build), '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=' + configuration]),
            ('build', ['cmake', '--build', str(build), '--parallel', '1']),
            ('gdi', [str(build / 'real_gdi_reference.exe'), font])]
        row = {'architecture': arch, 'configuration': configuration, 'phases': []}
        for phase, command in commands:
            with (out / (phase + '.log')).open('wb') as log:
                result = subprocess.run(command, cwd=out, env=environment, stdout=log, stderr=subprocess.STDOUT,
                                        timeout=90)
            row['phases'].append({'phase': phase, 'command': command, 'exit': result.returncode})
            if result.returncode:
                break
        if len(row['phases']) == 3 and row['phases'][-1]['exit'] == 0:
            image = build / 'real_gdi_reference.exe'
            data = image.read_bytes()
            pe = struct.unpack_from('<I', data, 0x3c)[0]
            assert struct.unpack_from('<H', data, pe + 4)[0] == machine
            shutil.copy2(image, out / image.name)
            shutil.copy2(build / 'compile_commands.json', out / 'compile_commands.json')
            shutil.copytree(build / 'extracted', out / 'extracted')
            row['binary_sha256'] = hashlib.sha256(data).hexdigest()
            row['pe_machine'] = hex(machine)
        rows.append(row)
        (a.output / 'cases.json').write_text(json.dumps(rows, indent=2) + '\n')
        print(arch, configuration, [(x['phase'], x['exit']) for x in row['phases']], flush=True)
raise SystemExit(int(any(len(x['phases']) != 3 or x['phases'][-1]['exit'] for x in rows)))
