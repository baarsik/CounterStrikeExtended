#!/usr/bin/env python3
"""Run actual retained-control fit/raster checks and an overflow negative control."""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

p = argparse.ArgumentParser()
p.add_argument('--runtime', type=Path, required=True)
p.add_argument('--platform', type=Path, required=True)
p.add_argument('--game', type=Path, required=True)
p.add_argument('--packet', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
p.add_argument('--executable', type=Path)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=False)
constraints = a.output.resolve() / 'controls.res'
subprocess.run([sys.executable, str(Path(__file__).with_name('prepare_fit.py')), '--output', str(constraints)], check=True)
executable = (a.executable or a.packet / 'vgui_tests').resolve()
runtime = a.runtime.resolve()
environment = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', SDL_RENDER_DRIVER='software')
results = {}
for language in ('russian', 'polish', 'negative'):
    output = a.output.resolve() / language
    command = [str(executable), str(runtime / 'vgui2.dylib'), str(runtime / 'filesystem_stdio.dylib'),
               str(a.platform.resolve()), str(a.game.resolve()), str((a.packet / 'resources').resolve()),
               str(constraints), str(output), 'polish' if language == 'negative' else language]
    if language == 'negative':
        command.append('negative')
    run = subprocess.run(command, env=environment, text=True, capture_output=True)
    (a.output / (language + '.log')).write_text(run.stdout + run.stderr)
    table = output / 'fit.tsv'
    rows = list(csv.DictReader(table.open(), delimiter='\t')) if table.exists() else []
    failures = [row for row in rows if row['result'] not in ('fit', 'elided-dynamic-summary')]
    expected = run.returncode != 0 and any(row['result'] == 'overflow' for row in rows) if language == 'negative' \
        else run.returncode == 0 and bool(rows) and not failures
    results[language] = {'exit': run.returncode, 'expected_result': expected, 'checks': len(rows),
                         'failures': failures, 'output': run.stdout.strip()}
    if language != 'negative' and shutil.which('sips'):
        for image in output.glob('*.bmp'):
            subprocess.run(['sips', '-s', 'format', 'png', str(image), '--out', str(image.with_suffix('.png'))],
                           check=True, capture_output=True)
    print(language + ':', 'PASS' if expected else 'FAIL', len(rows), 'measured constraints')
results['inputs'] = {'executable_sha256': hashlib.sha256(executable.read_bytes()).hexdigest(),
                     'vgui_sha256': hashlib.sha256((runtime / 'vgui2.dylib').read_bytes()).hexdigest(),
                     'constraints_sha256': hashlib.sha256(constraints.read_bytes()).hexdigest()}
(a.output / 'summary.json').write_text(json.dumps(results, ensure_ascii=False, indent=2) + '\n')
raise SystemExit(not all(results[language]['expected_result'] for language in ('russian', 'polish', 'negative')))
