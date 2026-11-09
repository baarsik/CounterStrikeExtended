#!/usr/bin/env python3
"""Seal exact fresh compiler evidence, actual cold fixtures and post-link pins; never run a game."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--contexts', type=Path, required=True, help='Directory holding release/debug actual contexts')
    parser.add_argument('--build-prefix', required=True)
    options = parser.parse_args()
    output = options.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    runs = []

    def run(label, command, environment=None, timeout=180):
        process = subprocess.run([str(value) for value in command], cwd=ROOT, capture_output=True,
                                 timeout=timeout, env=environment)
        (output / (label + '.log')).write_bytes(process.stdout + process.stderr)
        runs.append({'label': label, 'command': [str(value) for value in command], 'exit': process.returncode})
        (output / 'commands.json').write_text(json.dumps(runs, indent=2))
        if process.returncode != 0:
            raise RuntimeError(label + ': ' + (process.stdout + process.stderr).decode(errors='replace')[-6000:])

    evidence_dirs = {}
    for profile in ('release', 'debug'):
        directory = output / (profile + '-evidence')
        evidence_dirs[profile] = directory
        contexts = options.contexts / (profile + '-actual-contexts.json')
        run(profile + '-evidence', [sys.executable, ROOT / 'tools/vgui1_app_cold_evidence.py', 'generate',
            '--schema', ROOT / 'tools/vgui1_app_cold_evidence_schema.json', '--contexts', contexts,
            '--foundation', ROOT / 'GoldSrc-SDK/public/cse_vgui1_app_cold_abi.h', '--output', directory, '--metadata'])
        run(profile + '-actual-publisher', [sys.executable, ROOT / 'tests/native/vgui1_app_cold_admission/replay_publisher.py',
            '--evidence', directory, '--output', output / (profile + '-actual-publisher')])
    for profile, config, sanitize in [('release', 'Release', False), ('debug', 'Debug', False),
                                     ('sanitized', 'Debug', True)]:
        evidence = evidence_dirs['release' if config == 'Release' else 'debug']
        build = ROOT / 'build' / (options.build_prefix + '-' + profile)
        run(profile + '-configure', ['cmake', '-S', ROOT / 'tests/native/vgui1_app_cold_admission', '-B', build,
            '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=' + config, '-DCMAKE_OSX_ARCHITECTURES=arm64',
            '-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0', '-DCSE_APP_COLD_CONSTRUCTORS=ON',
            '-DCSE_APP_COLD_METADATA_HEADER=' + str(evidence / 'metadata.h'),
            '-DCSE_APP_COLD_SANITIZERS=' + ('ON' if sanitize else 'OFF')])
        run(profile + '-build', ['cmake', '--build', build, '--parallel', '4'])
        run(profile + '-ctest', ['ctest', '--test-dir', build, '--output-on-failure'])
        shutil.copytree(build / 'cold-controls', output / (profile + '-cold-controls'))
        images = output / (profile + '-images')
        images.mkdir()
        identities = []
        for name in ('reader_tests', 'app_cold_inspect', 'cold_host', 'libcold_counter.dylib',
                     'libcold_vgui.dylib', 'libcold_root.dylib'):
            image = build / name
            shutil.copy2(image, images / name)
            identities.append({'path': str(image), 'sha256': hashlib.sha256(image.read_bytes()).hexdigest()})
            run(profile + '-' + name + '-identity', ['/usr/bin/otool', '-hv', '-l', image])
        (output / (profile + '-images.json')).write_text(json.dumps(identities, indent=2))
        for kind, name in [('requirement', 'libcold_root.dylib'), ('offer', 'libcold_vgui.dylib')]:
            run(profile + '-' + kind + '-manifest', [sys.executable, ROOT / 'tools/vgui1_app_cold_manifest.py',
                '--image', build / name, '--kind', kind, '--evidence', evidence,
                '--inspector', build / 'app_cold_inspect', '--output', output / (profile + '-' + kind + '-manifest.json')])
    environment = dict(os.environ, CSE_APP_COLD_EVIDENCE=str(evidence_dirs['debug']),
                       CSE_APP_COLD_CONTEXTS=str(options.contexts / 'debug-actual-contexts.json'))
    run('tool-controls', [sys.executable, '-m', 'unittest', 'discover', '-s', 'tests/native/vgui1_app_cold_admission',
                         '-p', 'test_*.py', '-v'], environment)
    print('PASS actual R/D evidence + six actual publisher profiles + R/D/sanitized reader/cold/manifest controls')


if __name__ == '__main__':
    main()
