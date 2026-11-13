#!/usr/bin/env python3
"""Syntax-check changed CS TUs using the existing production compile database; never invoke Ninja."""
from pathlib import Path
import argparse
import json
import shlex
import subprocess

root = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser()
parser.add_argument('--database', type=Path, default=root / 'build/runtime-macos-arm64-release/compile_commands.json')
args = parser.parse_args()
data = json.loads(args.database.read_text())
changed = subprocess.check_output(['git', 'diff', '--name-only', '--', 'src/cstrike'], cwd=root, text=True).splitlines()
checked = []
failures = []
for filename in changed:
    if not filename.endswith('.cpp'):
        continue
    entry = next((row for row in data if row['file'] == str(root / filename)), None)
    if not entry:
        continue
    command = shlex.split(entry['command'])
    index = command.index('-o')
    del command[index:index + 2]
    command.remove('-c')
    command.append('-fsyntax-only')
    result = subprocess.run(command, cwd=entry['directory'], capture_output=True, text=True)
    checked.append(filename)
    if result.returncode:
        failures.append(filename)
        print(result.stderr)
report = root / 'build/live-localization/cs-syntax-results.txt'
report.parent.mkdir(parents=True, exist_ok=True)
report.write_text('\n'.join(checked) + '\nFailures: ' + str(failures) + '\n')
print(f'CS strict production syntax: {len(checked)} TUs, {len(failures)} failures.')
raise SystemExit(bool(failures))
