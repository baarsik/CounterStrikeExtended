"""Run the separately authorized immutable loopback candidates with a process wall-time limit."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--packet', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--execute', action='store_true', help='Use only after the separate network fixture approval')
args = parser.parse_args()
manifest = json.loads((args.packet / 'macos-binaries.json').read_text())
assert not args.output.exists(), 'Receipts must be new'
args.output.mkdir(parents=True)
results = []
for row in manifest['configurations']:
    binary = args.packet / row['loopback_binary']
    assert hashlib.sha256(binary.read_bytes()).hexdigest() == row['loopback_sha256'], binary
    mode = '--loopback' if args.execute else '--describe'
    environment = dict(os.environ)
    environment['ASAN_OPTIONS'] = 'halt_on_error=1'
    environment['UBSAN_OPTIONS'] = 'halt_on_error=1'
    record = {'configuration': row['configuration'], 'binary_sha256': row['loopback_sha256'],
              'mode': mode, 'process_timeout_seconds': 8}
    try:
        completed = subprocess.run([str(binary), mode], env=environment, capture_output=True, timeout=8)
        record['exit_code'] = completed.returncode
        (args.output / (row['configuration'] + '.stdout')).write_bytes(completed.stdout)
        (args.output / (row['configuration'] + '.stderr')).write_bytes(completed.stderr)
        expected = b'real_ipv4_loopback_passed=1' if args.execute else b'build_only=1'
        record['passed'] = completed.returncode == 0 and expected in completed.stdout
    except subprocess.TimeoutExpired as error:
        record['timeout'] = True
        record['passed'] = False
        (args.output / (row['configuration'] + '.stdout')).write_bytes(error.stdout or b'')
        (args.output / (row['configuration'] + '.stderr')).write_bytes(error.stderr or b'')
    results.append(record)
    (args.output / 'result.json').write_text(json.dumps({'network_execution': args.execute, 'cases': results}, indent=2) + '\n')
raise SystemExit(int(not all(row['passed'] for row in results)))
