#!/usr/bin/env python3
"""Actual cold-constructor controls, with immutable pins before each explicit load."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile


def run(command, output, timeout=45):
    result = subprocess.run([str(value) for value in command], capture_output=True, timeout=timeout)
    output.write_text(json.dumps({'command': [str(value) for value in command], 'exit': result.returncode,
        'stdout': result.stdout.decode(errors='replace'), 'stderr': result.stderr.decode(errors='replace')}, indent=2))
    if result.returncode != 0:
        raise RuntimeError(output.read_text())
    return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    options = parser.parse_args()
    build = options.build.resolve()
    options.output.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='run-', dir=options.output.resolve()))
    inspector = build / 'app_cold_inspect'
    root = build / 'libcold_root.dylib'
    provider = build / 'libcold_vgui.dylib'
    initial = {str(path): hashlib.sha256(path.read_bytes()).hexdigest()
               for path in (inspector, root, provider, build / 'cold_host', build / 'libcold_counter.dylib')}

    def facts(path, label):
        return json.loads(run([inspector, path], output / (label + '-inspection.json')))

    def arguments(mode, offered, label):
        command = [build / 'cold_host', mode]
        for path, item in ((root, facts(root, label + '-root')), (offered, facts(offered, label + '-offer'))):
            command += [path, str(item['bytes']), item['uuid'], item['sha256'], item['record_sha256']]
        return command

    # Valid byte mutation, never executed: mismatching offered catalog must be
    # rejected before even the real consumer's counter-marked constructor runs.
    altered = output / 'mismatched-never-loaded.dylib'
    raw = bytearray(provider.read_bytes())
    original = facts(provider, 'original-provider')
    signature = original['metadata_offset'] + 64 + 32 + 2 * 16 + 8
    raw[signature] ^= 1
    altered.write_bytes(raw)
    run(arguments('--reject', altered, 'mismatch'), output / 'mismatch-before-constructors.json')
    run(arguments('--load', provider, 'matching'), output / 'matching-constructors.json')
    final = {path: hashlib.sha256(Path(path).read_bytes()).hexdigest() for path in initial}
    if initial != final:
        raise RuntimeError('Image drift during controls')
    (output / 'identities.json').write_text(json.dumps({'before': initial, 'after': final}, indent=2))
    print('PASS mismatch-before-constructors + matching complete cold state + actual UUID/path + normal unload')


if __name__ == '__main__':
    main()
