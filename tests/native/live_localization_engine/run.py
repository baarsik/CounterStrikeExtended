#!/usr/bin/env python3
"""Run actual engine/controller, titles, filesystem and renderer localization owners."""
import argparse
import importlib.util
import json
from pathlib import Path
import subprocess
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--evidence', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
out = args.evidence.resolve()
out.mkdir(parents=True, exist_ok=True)
if sys.platform != 'darwin':
    raise SystemExit('This owner fixture selects the macOS filesystem provider; use the Windows fixture separately.')
commands = []

def run(command):
    commands.append([str(value) for value in command])
    subprocess.run(commands[-1], cwd=root, check=True)

run(['python3', root / 'tools/build_localization.py', '--source', root / 'src/deploy/localization',
     '--output', out / 'runtime', '--language-header', out / 'InterfaceLanguages.generated.inc'])
flags = ['c++', '-std=c++17', '-g', '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
         '-I', root / 'src/engine', '-I', root / 'src/public', '-I', out]
for name in ('state', 'title'):
    executable = out / (name + '_tests')
    run(flags + ['-Wall', '-Wextra', '-Werror', root / 'tests/native/live_localization_engine' / (name + '_tests.cpp'),
                 '-o', executable])
    run([executable])
spec = importlib.util.spec_from_file_location('filesystem_manifest', root / 'tests/native/filesystem/manifest_sources.py')
manifest = importlib.util.module_from_spec(spec)
spec.loader.exec_module(manifest)
sources, policy = manifest.sources(root, 'macos', 'filesystem_stdio', 'arm64')
(out / 'filesystem-source-policy.json').write_text(json.dumps(policy, indent=2) + '\n')
executable = out / 'filesystem_tests'
run(flags + ['-Wno-nontrivial-memcall', '-Wno-deprecated-declarations', '-I', root / 'src/filesystem',
             root / 'tests/native/live_localization_engine/filesystem_tests.cpp'] + sources + ['-o', executable])
run([executable, out / 'filesystem-data'])
for script, directory in (('center_tests.py', 'center'), ('font_reset_tests.py', 'font-reset')):
    run(['python3', root / 'tests/native/live_localization_engine' / script, '--evidence', out / directory])
(out / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
print('PASS: live localization engine owner fixtures')
