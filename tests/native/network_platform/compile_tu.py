"""Replay the actual generated transport compile row without linking or initializing providers."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--database', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--sanitizers', action='store_true')
args = parser.parse_args()
rows = [row for row in json.loads(args.database.read_text())
        if row['file'].endswith('/engine/net_ws.cpp') and '/hw_engine/' in row['output']]
assert len(rows) == 1
row = rows[0]
command = shlex.split(row['command'])
command[command.index('-o') + 1] = str(args.output)
command += ['-MMD', '-MF', str(args.output) + '.d']
if args.sanitizers:
    command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-sanitize-recover=all']
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.with_suffix('.command.json').write_text(json.dumps({'directory': row['directory'], 'arguments': command}, indent=2) + '\n')
result = subprocess.run(command, cwd=row['directory'], capture_output=True)
args.output.with_suffix('.log').write_bytes(result.stdout + result.stderr)
raise SystemExit(result.returncode)
