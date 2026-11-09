#!/usr/bin/env python3
"""Recompile full owning NetSocket TU recipes; no syscall hooks, linking or runtime execution."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--recipes', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    entries = json.loads(args.recipes.read_text())
    rows = []
    for profile in ['release', 'debug', 'sanitizers']:
        entry = next(row for row in entries if row['label'] == 'NetSocket-' + ('debug' if profile == 'debug' else 'release'))
        command = entry['args'].copy()
        command[command.index('-o') + 1] = str(args.output / (profile + '.o'))
        if profile == 'sanitizers':
            command += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
        result = subprocess.run(command, cwd=entry['cwd'], capture_output=True, timeout=120)
        (args.output / (profile + '.log')).write_bytes(result.stdout + result.stderr)
        rows.append({'profile': profile, 'args': command, 'cwd': entry['cwd'], 'exit': result.returncode,
                     'source_sha256': hashlib.sha256(Path(command[command.index('-c') + 1]).read_bytes()).hexdigest()})
        print(profile, result.returncode)
    (args.output / 'receipt.json').write_text(json.dumps(rows, indent=2))
    assert all(row['exit'] == 0 for row in rows)


if __name__ == '__main__':
    main()
