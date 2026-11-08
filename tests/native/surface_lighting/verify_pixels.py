"""Run the actual lighting fixture and require the frozen Win32 MMX byte result."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

if __name__ == '__main__':
    executable, output, baseline_path = sys.argv[1:]
    baseline = json.loads(Path(baseline_path).read_text())
    fixture = Path(__file__).with_name('surface_tests.cpp')
    if hashlib.sha256(fixture.read_bytes()).hexdigest() != baseline['fixture_source_sha256']:
        sys.exit('Surface corpus inputs changed: rebuild the pinned MMX reference before updating the baseline')
    result = subprocess.run([executable, output], check=False)
    if result.returncode:
        sys.exit(result.returncode)
    data = Path(output).read_bytes()
    if len(data) != baseline['bytes'] or hashlib.sha256(data).hexdigest() != baseline['sha256']:
        sys.exit('Surface pixels/state differ from the frozen Win32 MMX reference')
    print('Exact frozen Win32 MMX pixels, state and guards match')
