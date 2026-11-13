#!/usr/bin/env python3
"""Compile-only include closure receipts for the exact already-executed commands."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--art', type=Path, required=True)
parser.add_argument('--work', type=Path, required=True)
args = parser.parse_args()
art = args.art.resolve()
work = args.work.resolve()
work.mkdir(parents=True, exist_ok=True)
out = art / 'windows-closure'
out.mkdir(exist_ok=True)
receipts = []
for cpu in ['x86', 'x64', 'arm64']:
    setup = work / ('setup-' + cpu + '.cmd')
    setup.write_text('@echo off\ncall "C:\\BuildTools\\2026\\Common7\\Tools\\VsDevCmd.bat" '
        '-no_logo -host_arch=arm64 -arch=' + cpu + ' >nul\nif errorlevel 1 exit /b 1\nset\n')
    result = subprocess.run(['cmd.exe', '/d', '/c', setup], capture_output=True, text=True, check=True)
    env = dict(os.environ)
    for line in result.stdout.splitlines():
        if '=' in line and not line.startswith('='):
            key, value = line.split('=', 1)
            env[key] = value
    for cohort in ['windows', 'windows-component']:
        rows = json.loads((art / cohort / 'commands.json').read_text())
        for row in rows:
            if row['phase'] != 'compile' or not row['context'].startswith(cpu + '-'):
                continue
            tag = cohort + '-' + row['context']
            build = work / tag
            build.mkdir(exist_ok=True)
            dependency = build / 'dependencies.json'
            command = [item for item in row['command'] if not item.startswith('/Fe')]
            command += ['/c', '/Fo' + str(build / 'fixture.obj'), '/sourceDependencies', str(dependency)]
            result = subprocess.run(command, cwd=build, env=env, capture_output=True, timeout=90)
            (out / (tag + '.log')).write_bytes(result.stdout + result.stderr)
            assert result.returncode == 0, (tag, result.stderr)
            data = json.loads(dependency.read_text(encoding='utf-8-sig'))
            assert data['Version'] == '1.2' and isinstance(data['Data']['Includes'], list), data
            shutil.copy2(dependency, out / (tag + '-raw.json'))
            closure = []
            for name in [data['Data']['Source']] + data['Data']['Includes']:
                path = Path(name)
                digest = hashlib.sha256(path.read_bytes()).hexdigest()
                closure.append({'path': str(path), 'sha256': digest})
                # Preserve actual owned declaration and controlled fixture inputs;
                # system/toolchain includes retain exact hashes without SDK copying.
                marker = '\\src\\'
                lowered = str(path).lower()
                if marker in lowered:
                    index = lowered.index(marker)
                    relative = Path(str(path)[index + 1:])
                    target = out / 'owned-inputs' / relative
                    target.parent.mkdir(parents=True, exist_ok=True)
                    if target.exists():
                        assert hashlib.sha256(target.read_bytes()).hexdigest() == digest, target
                    else:
                        shutil.copy2(path, target)
            receipts.append({'tag': tag, 'command': command, 'exit': result.returncode, 'closure': closure})
            print('PASS', tag, 'actual include closure', len(closure), flush=True)
(out / 'receipts.json').write_text(json.dumps(receipts, indent=2) + '\n')
