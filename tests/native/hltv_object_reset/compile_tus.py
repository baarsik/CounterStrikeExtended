#!/usr/bin/env python3
"""Compile complete actual owned production TUs with the owning generated recipe, without linking/running HLTV."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[3]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--release-database', type=Path, required=True)
    parser.add_argument('--debug-database', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    options = parser.parse_args()
    options.output.mkdir(parents=True, exist_ok=True)
    receipts = []
    for name in ['Network', 'World', 'NetAddress']:
        for profile in ['release', 'debug', 'sanitizers']:
            database = options.debug_database if profile == 'debug' else options.release_database
            candidates = [row for row in json.loads(database.read_text()) if
                          row['file'] == str(ROOT / 'GoldSrc-SDK/HLTV' /
                          ('common' if name == 'NetAddress' else 'Core') / (name + '.cpp'))]
            entry = next(row for row in candidates if 'targets/hltv_core/' in row['output'])
            command = shlex.split(entry['command'])
            label = name + '-' + profile
            command[command.index('-o') + 1] = str(options.output / (label + '.o'))
            if profile == 'sanitizers':
                command += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
            result = subprocess.run(command, cwd=entry['directory'], capture_output=True, timeout=120)
            (options.output / (label + '.log')).write_bytes(result.stdout + result.stderr)
            receipts.append({'label': label, 'args': command, 'cwd': entry['directory'], 'exit': result.returncode,
                             'source_sha256': hashlib.sha256(Path(entry['file']).read_bytes()).hexdigest(),
                             'command_database_sha256': hashlib.sha256(database.read_bytes()).hexdigest()})
            print(label, result.returncode, flush=True)
    (options.output / 'receipt.json').write_text(json.dumps(receipts, indent=2))
    assert all(row['exit'] == 0 for row in receipts), 'whole-TU failures retained in logs'


if __name__ == '__main__':
    main()
