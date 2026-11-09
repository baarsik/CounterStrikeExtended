"""Verify the exact offline device-probe inputs, without compiling at runtime."""
from pathlib import Path
import hashlib
import json

root = Path(__file__).resolve().parent / 'shaders'
provenance = json.loads((root / 'provenance.json').read_text())
for name, record in provenance['files'].items():
    actual = hashlib.sha256((root / name).read_bytes()).hexdigest()
    assert actual == record['sha256'], name
    print(name, actual)
