"""Bound fixture waits and verify joins performed during real global destruction."""
import argparse
from pathlib import Path
import subprocess
import time

parser = argparse.ArgumentParser()
parser.add_argument('binary', type=Path)
parser.add_argument('scenario')
parser.add_argument('directory', type=Path)
args = parser.parse_args()
args.directory.mkdir(parents=True, exist_ok=True)
marker = args.directory / 'joins.bin'
marker.unlink(missing_ok=True)
deadline = time.monotonic() + 8
process = subprocess.Popen([str(args.binary), args.scenario, str(marker)], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
try:
    stdout, stderr = process.communicate(timeout=max(0, deadline - time.monotonic()))
except subprocess.TimeoutExpired:
    process.kill()
    stdout, stderr = process.communicate()
    (args.directory / 'stdout.log').write_bytes(stdout)
    (args.directory / 'stderr.log').write_bytes(stderr)
    raise RuntimeError('Absolute fixture deadline exceeded; isolated child terminated')
(args.directory / 'stdout.log').write_bytes(stdout)
(args.directory / 'stderr.log').write_bytes(stderr)
assert process.returncode == 0, (process.returncode, stderr.decode(errors='replace'))
expected = {'initial': 0, 'lifecycle': 100, 'stop': 2, 'failure': 2, 'invalid': 2, 'queries': 100, 'destruction': 1, 'tls': 1, 'spurious': 50}[args.scenario]
observed = marker.read_bytes() if marker.exists() else b''
assert observed == b'J' * expected, (args.scenario, len(observed), expected)
print(stdout.decode(), end='')
print('joins_after_global_destruction=' + str(len(observed)))
