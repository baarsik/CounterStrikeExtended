"""Replay each actual owning filesystem compiler row; never duplicate its source inventory."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--database', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--sanitizers', action='store_true')
parser.add_argument('--file', choices=['BaseFileSystem.cpp', 'FileSystem_Stdio.cpp', 'dbg.cpp', 'pathmatch.cpp'])
parser.add_argument('--source', type=Path)
parser.add_argument('--controlled-clock', action='store_true')
parser.add_argument('--legacy-reference', action='store_true')
args = parser.parse_args()
rows = json.loads(args.database.read_text())
args.output.mkdir(parents=True, exist_ok=True)
receipts = []
for name in ('BaseFileSystem.cpp', 'FileSystem_Stdio.cpp', 'dbg.cpp', 'pathmatch.cpp'):
    if args.file and args.file != name:
        continue
    selected = [x for x in rows if x['file'].endswith('/filesystem/' + name)]
    assert len(selected) == 1, name
    row = selected[0]
    output = args.output / (name + '.o')
    command = shlex.split(row['command'])
    source = args.source or Path(row['file'])
    if args.source:
        command[command.index(row['file'])] = str(args.source)
    if args.controlled_clock:
        assert name == 'dbg.cpp'
        command += ['-Dgettimeofday=CseFixtureGettimeofday']
    if args.legacy_reference:
        assert args.source and args.source.resolve() != Path(row['file']).resolve()
        command += ['-Wno-error=shorten-64-to-32']
    command[command.index('-o') + 1] = str(output)
    command += ['-MMD', '-MF', str(output) + '.d']
    if args.sanitizers:
        command += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
    completed = subprocess.run(command, cwd=row['directory'], capture_output=True)
    (args.output / (name + '.log')).write_bytes(completed.stdout + completed.stderr)
    receipts.append({'file': row['file'], 'actual_source': str(source), 'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
                     'arguments': command, 'directory': row['directory'], 'exit_code': completed.returncode})
(args.output / 'commands.json').write_text(json.dumps(receipts, indent=2) + '\n')
raise SystemExit(int(any(x['exit_code'] for x in receipts)))
