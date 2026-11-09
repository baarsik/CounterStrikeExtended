"""Run bounded actual caller fragments and native prefix controls, without UI/providers."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import shutil
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--output', type=Path, required=True)
p.add_argument('--build-root', type=Path, required=True)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=False)
source = Path(__file__).resolve().parent
rows = []
for name, configuration, extra in [('release', 'Release', []), ('debug', 'Debug', []),
                                  ('sanitized', 'RelWithDebInfo', ['-DCSE_CALLER_SANITIZERS=ON'])]:
    output = a.output / name
    output.mkdir()
    build = a.build_root.resolve() / name
    commands = [('configure', ['cmake', '-S', str(source), '-B', str(build), '-G', 'Ninja',
        '-DCMAKE_BUILD_TYPE=' + configuration, '-DCMAKE_OSX_ARCHITECTURES=arm64',
        '-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0'] + extra),
        ('build', ['cmake', '--build', str(build), '--parallel', '3']),
        ('tests', ['ctest', '--test-dir', str(build), '--output-on-failure'])]
    row = {'profile': name, 'phases': []}
    for phase, command in commands:
        with (output / (phase + '.stdout')).open('wb') as stdout, (output / (phase + '.stderr')).open('wb') as stderr:
            result = subprocess.run(command, stdout=stdout, stderr=stderr, timeout=90, check=False)
        row['phases'].append({'phase': phase, 'command': command, 'exit': result.returncode})
        if result.returncode:
            break
    if len(row['phases']) == 3 and row['phases'][-1]['exit'] == 0:
        shutil.copytree(build / 'extracted', output / 'extracted')
        shutil.copy2(build / 'compile_commands.json', output / 'compile_commands.json')
        for binary in ['chat_controls', 'alias_controls']:
            path = build / binary
            shutil.copy2(path, output / binary)
            row[binary] = {'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                'machine': subprocess.check_output(['file', str(path)], text=True),
                'load_commands': subprocess.check_output(['otool', '-l', str(path)], text=True)}
    rows.append(row)
    (a.output / 'cases.json').write_text(json.dumps(rows, indent=2) + '\n')
    print(name, [(x['phase'], x['exit']) for x in row['phases']], flush=True)
(a.output / 'host.json').write_text(json.dumps({'platform': platform.platform(), 'machine': platform.machine()}, indent=2) + '\n')
raise SystemExit(int(any(len(x['phases']) != 3 or x['phases'][-1]['exit'] for x in rows)))
