"""Run the real launcher entry before module/game startup, in an empty owned directory."""
from pathlib import Path
import subprocess
import sys
import tempfile

launcher = str(Path(sys.argv[1]).resolve())
limit = 4095

def run(arguments):
    with tempfile.TemporaryDirectory(prefix='cse-launcher-entry-') as directory:
        return subprocess.run([launcher, *arguments], cwd=directory, capture_output=True, timeout=10)

for arguments in [['x' * (limit + 1)], ['x ' * (limit // 2), 'second']]:
    result = run(arguments)
    assert result.returncode == 1, (result.returncode, result.stderr.decode(errors='replace'))
    assert b'Command line exceeds 4095 bytes' in result.stderr
    assert b'Failed to load filesystem' not in result.stderr

encoded_launcher_bytes = len(launcher.encode()) + (2 if ' ' in launcher else 0)
result = run(['x' * (limit - encoded_launcher_bytes - 1)])
assert result.returncode == 0, (result.returncode, result.stderr.decode(errors='replace'))
assert b'Failed to load filesystem library' in result.stderr
print('launcher_entry_argument_cases=3; actual module/game startup excluded')
