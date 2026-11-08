#!/usr/bin/env python3
"""Check the canonical decoder fixture's encoded input and exact PCM bytes."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

DEFAULT_REFERENCE = Path(__file__).resolve().parents[2] / 'tests/music/pcm-reference.json'


def verify(encoded, pcm, reference):
    if reference.get('schema') != 1:
        raise ValueError('Unsupported PCM reference schema')
    if hashlib.sha256(encoded).hexdigest() != reference['encoded_sha256']:
        raise ValueError('Canonical encoded input hash mismatch')
    if len(pcm) != reference['pcm_bytes'] or hashlib.sha256(pcm).hexdigest() != reference['pcm_sha256']:
        raise ValueError('Canonical decoder PCM differs; exact bytes are required')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--encoded', type=Path, required=True)
    parser.add_argument('--pcm', type=Path, required=True)
    parser.add_argument('--reference', type=Path, default=DEFAULT_REFERENCE)
    args = parser.parse_args()
    try:
        reference = json.loads(args.reference.read_text(encoding='utf-8'))
        pcm = args.pcm.read_bytes()
        verify(args.encoded.read_bytes(), pcm, reference)
        print(f"Canonical PCM matches: {len(pcm)} bytes, SHA256 {reference['pcm_sha256']}")
    except (OSError, ValueError, KeyError) as error:
        print(str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
