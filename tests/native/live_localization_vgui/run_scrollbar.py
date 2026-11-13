#!/usr/bin/env python3
"""Compile the actual shared/private scrollbar mapping methods with controlled panel geometry."""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import os
import subprocess

root = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser()
parser.add_argument('--evidence', type=Path, required=True)
args = parser.parse_args()
out = args.evidence.resolve()
out.mkdir(parents=True, exist_ok=True)
spec = importlib.util.spec_from_file_location('extract', root / 'tests/native/network_platform/extract_functions.py')
extract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(extract)
results = []
for variant, source in (('shared', root / 'src/vgui2/controls/ScrollBarSlider.cpp'),
                        ('cs', root / 'src/cstrike/vgui2/controls/ScrollBarSlider.cpp')):
    folder = out / variant
    folder.mkdir(exist_ok=True)
    text = source.read_bytes().decode('latin1')
    names = ('ScrollBarSlider::RecomputeNobPosFromValue', 'ScrollBarSlider::RecomputeValueFromNobPos')
    (folder / 'scrollbar_geometry.inc').write_text('\n'.join(extract.extract(text, name) for name in names))
    command = ['c++', '-std=c++17', '-g', '-O1', '-Wall', '-Wextra', '-Werror',
               '-fsanitize=address,undefined,float-cast-overflow', '-fno-omit-frame-pointer',
               '-I', str(folder), str(root / 'tests/native/live_localization_vgui/scrollbar_geometry_tests.cpp'),
               '-o', str(folder / 'scrollbar_geometry_tests')]
    subprocess.run(command, check=True)
    run = subprocess.run([str(folder / 'scrollbar_geometry_tests')], capture_output=True, text=True,
                         env=dict(os.environ, UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
    (folder / 'run.log').write_text(run.stdout + run.stderr)
    results.append({'variant': variant, 'source': str(source), 'sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
                    'compile': command, 'exit': run.returncode})
    (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    print(variant, run.returncode)
    print(run.stdout + run.stderr, end='')
    if run.returncode:
        raise SystemExit(1)
