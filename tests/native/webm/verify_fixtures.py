"""Verify owned encoded media and independent references before native decode."""

import hashlib
import json
from pathlib import Path


if __name__ == '__main__':
    directory = Path(__file__).parent / 'fixtures'
    manifest = json.loads((directory / 'manifest.json').read_text())
    for name, expected in manifest['sha256'].items():
        actual = hashlib.sha256((directory / name).read_bytes()).hexdigest()
        if actual != expected:
            raise SystemExit('Owned WebM fixture hash mismatch: ' + name)
    print('Owned VP8/VP9/Vorbis files and independent decoded references match known hashes.')
