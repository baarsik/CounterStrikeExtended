#!/usr/bin/env python3
"""Recompile pinned actual console TU recipes and explicitly derived new-source profiles."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--pinned-recipes', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    options = parser.parse_args()
    options.output.mkdir(parents=True, exist_ok=True)
    recipes = json.loads(options.pinned_recipes.read_text())
    results = []
    for source in ['System', 'TextConsole', 'PosixSystem', 'TextConsolePosix']:
        for profile in ['release', 'debug', 'sanitizers']:
            original = next(row for row in recipes if row['label'] ==
                            (source if source in ['System', 'TextConsole'] else 'TextConsole') + '-' +
                            ('release' if profile == 'sanitizers' else profile))
            args = original['args'].copy()
            args[-1] = str(ROOT / 'src/HLTV/Console' / (source + '.cpp'))
            args[args.index('-o') + 1] = str(options.output / (source + '-' + profile + '.o'))
            if profile == 'sanitizers':
                args += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
            result = subprocess.run(args, cwd=original['cwd'], capture_output=True, timeout=120)
            label = source + '-' + profile
            (options.output / (label + '.log')).write_bytes(result.stdout + result.stderr)
            results.append({'label': label, 'args': args, 'cwd': original['cwd'], 'exit': result.returncode,
                            'source_sha256': hashlib.sha256(Path(args[args.index('-c') + 1]).read_bytes()).hexdigest(),
                            'recipe': 'actual retained TU' if source in ['System', 'TextConsole'] else
                            'derived new source using actual retained TextConsole target profile; metadata integration pending'})
            print(label, result.returncode, flush=True)
    (options.output / 'receipt.json').write_text(json.dumps(results, indent=2))
    assert all(row['exit'] == 0 for row in results), 'whole-TU compile failure; retain full logs'


if __name__ == '__main__':
    main()
