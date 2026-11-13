#!/usr/bin/env python3
"""Run actual extracted HL HUD owners with actual engine-owned credits font metrics."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

CASES = ('russian', 'polish', 'effects', 'timing', 'multiline', 'long-line',
         'no-final-newline', 'clipping', 'byte-fallback', 'service-lifetime')
parser = argparse.ArgumentParser()
parser.add_argument('--root', type=Path, default=Path('.'))
parser.add_argument('--evidence', type=Path, required=True)
parser.add_argument('--runtime', type=Path, required=True)
parser.add_argument('--resources', type=Path, required=True, help='Retained valve directory containing resource/TrackerScheme.res')
parser.add_argument('--sanitize', action='store_true')
parser.add_argument('--source', type=Path, help='Explicit pinned message.cpp for behavioral RED comparison')
parser.add_argument('--cases', nargs='+', choices=CASES, default=CASES)
parser.add_argument('--expect-failure', action='store_true')
parser.add_argument('--font-mode', choices=('scaled', 'legacy', 'both'), default='scaled')
args = parser.parse_args()
root = args.root.resolve()
out = args.evidence.resolve()
runtime = args.runtime.resolve()
resources = args.resources.resolve()
out.mkdir(parents=True, exist_ok=True)
extract = ['python3', str(root / 'tests/native/hl_message_utf8/extract.py'), str(root), str(out)]
if args.source:
    extract.append(str(args.source.resolve()))
subprocess.run(extract, check=True)
command = ['c++', '-std=c++17', '-g', '-O1', '-Wno-writable-strings', '-Wno-deprecated-declarations',
           '-Wl,-export_dynamic', str(root / 'tests/native/hl_message_utf8/hud_tests.cpp'),
           str(root / 'tests/native/hl_message_utf8/keyvalues_layout.cpp'), '-o', str(out / 'hud_tests')]
for directory in (out, root / 'src/vgui2/src/public', root / 'src/cl_dll', root / 'src/public',
                  root / 'src/common', root / 'src/engine', root / 'src/pm_shared', root / 'src/game_shared',
                  root / 'src/dlls'):
    command.extend(['-I', str(directory)])
if args.sanitize:
    command.extend(['-fsanitize=address,undefined', '-fno-omit-frame-pointer'])
(out / 'compile-command.json').write_text(json.dumps(command, indent=2) + '\n')
subprocess.run(command, check=True)
resource_modes = []
if args.font_mode in ('scaled', 'both'):
    resource_modes.append(('scaled', resources))
if args.font_mode in ('legacy', 'both'):
    legacy = out / 'legacy-font-resources'
    (legacy / 'resource').mkdir(parents=True, exist_ok=True)
    source = (resources / 'resource/TrackerScheme.res').read_text()
    changed, count = re.subn(r'("UseScaledCreditsFont"\s*)"1"', r'\g<1>"0"', source)
    if count != 1:
        raise SystemExit('Expected one actual retained UseScaledCreditsFont=1 setting')
    (legacy / 'resource/TrackerScheme.res').write_text(changed)
    resource_modes.append(('legacy', legacy))
results = []
for mode, directory in resource_modes:
    for case in args.cases:
        completed = subprocess.run([str(out / 'hud_tests'), str(runtime), str(directory), case],
                                   cwd=runtime, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (out / f'{mode}-{case}.log').write_text(completed.stdout)
        print(f'{mode}/{case}: exit={completed.returncode}')
        print(completed.stdout, end='')
        results.append({'font_mode': mode, 'case': case, 'exit_code': completed.returncode})
(out / 'provenance.json').write_text(json.dumps({str(path): hashlib.sha256(path.read_bytes()).hexdigest()
    for path in (resources / 'resource/TrackerScheme.res', runtime / 'hw.dylib', runtime / 'vgui2.dylib',
                 runtime / 'filesystem_stdio.dylib')}, indent=2) + '\n')
(out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
if any((result['exit_code'] == 0) == args.expect_failure for result in results):
    raise SystemExit(1)
