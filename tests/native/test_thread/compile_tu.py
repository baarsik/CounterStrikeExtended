"""Replay the actual owning testthread.cpp compiler command, with explicit fixture controls."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--database', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--source', type=Path)
parser.add_argument('--sanitizers', choices=['address', 'thread'])
parser.add_argument('--controlled-create', action='store_true')
args = parser.parse_args()
rows = [x for x in json.loads(args.database.read_text()) if x['file'].endswith('/tier0/testthread.cpp')]
assert len(rows) == 1
row = rows[0]
command = shlex.split(row['command'])
if args.source:
    command[command.index(row['file'])] = str(args.source)
command[command.index('-o') + 1] = str(args.output)
command += ['-MMD', '-MF', str(args.output) + '.d']
if args.controlled_create:
    command += ['-Dpthread_create=CseTestFixtureCreate', '-Dpthread_join=CseTestFixtureJoin',
                '-Dpthread_cond_wait=CseTestFixtureConditionWait']
if args.sanitizers:
    flags = 'address,undefined' if args.sanitizers == 'address' else 'thread'
    command += ['-fsanitize=' + flags, '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
args.output.parent.mkdir(parents=True, exist_ok=True)
result = subprocess.run(command, cwd=row['directory'], capture_output=True)
Path(str(args.output) + '.log').write_bytes(result.stdout + result.stderr)
Path(str(args.output) + '.command.json').write_text(json.dumps({'source_sha256': hashlib.sha256((args.source or Path(row['file'])).read_bytes()).hexdigest(), 'arguments': command, 'directory': row['directory'], 'exit_code': result.returncode}, indent=2) + '\n')
raise SystemExit(result.returncode)
