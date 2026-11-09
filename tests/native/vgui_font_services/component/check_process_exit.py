"""Check actual executable exit ordering, rather than only final font absence."""
from pathlib import Path
import subprocess
import sys

result = subprocess.run(sys.argv[1:] + ['exit'], capture_output=True, text=True, timeout=10)
sys.stdout.write(result.stdout)
sys.stderr.write(result.stderr)
assert result.returncode == 0, result.returncode
lines = Path(sys.argv[-1]).read_text().splitlines()
assert len(lines) == 4, lines
assert lines[0] == 'context-destroyed faces=0 created=1 destroyed=1 registrations=1 releases=1', lines
assert set(lines[1:3]) == {'consumer=a live-metric=5', 'consumer=b live-metric=5'}, lines
assert lines[3] == 'context-destroyed faces=0 created=2 destroyed=2 registrations=2 releases=2', lines
print('PASS actual exit: both client destructors use live font before service releases context')
