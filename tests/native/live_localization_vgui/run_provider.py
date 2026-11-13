#!/usr/bin/env python3
"""Link provider-only assertions with the admitted owning-control fixture packet."""
import argparse
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--packet', type=Path, required=True)
parser.add_argument('--evidence', type=Path, required=True)
parser.add_argument('--vgui', type=Path, required=True)
parser.add_argument('--filesystem', type=Path, required=True)
parser.add_argument('--platform', type=Path, required=True)
parser.add_argument('--sanitizers', action='store_true')
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
packet = args.packet.resolve()
out = args.evidence.resolve()
out.mkdir(parents=True, exist_ok=True)
entries = json.loads((packet / 'commands.json').read_text())
driver = next(entry for entry in entries if entry['source'].endswith('/runtime_tests.cpp'))
command = driver['argv'][:]
command[command.index('-c') + 1] = str(root / 'tests/native/live_localization_vgui/provider_tests.cpp')
command[command.index('-o') + 1] = str(out / 'provider.o')
if '-MF' in command:
    command[command.index('-MF') + 1] = str(out / 'provider.d')
if args.sanitizers:
    command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
result = subprocess.run(command, cwd=driver['cwd'], text=True, capture_output=True)
(out / 'compile.log').write_text(result.stdout + result.stderr)
if result.returncode:
    raise SystemExit(result.stderr)
link = json.loads((packet / 'link-command.json').read_text())['argv']
link[link.index('-o') + 1] = str(out / 'provider-tests')
link[link.index(driver['object'])] = str(out / 'provider.o')
if args.sanitizers:
    link += ['-fsanitize=address,undefined']
result = subprocess.run(link, text=True, capture_output=True)
(out / 'link.log').write_text(result.stdout + result.stderr)
if result.returncode:
    raise SystemExit(result.stderr)
run = [str(out / 'provider-tests'), str(args.vgui.resolve()), str(args.filesystem.resolve()),
       str(args.platform.resolve()), str(out / 'runtime')]
result = subprocess.run(run, text=True, capture_output=True)
(out / 'run.log').write_text(result.stdout + result.stderr)
(out / 'commands.json').write_text(json.dumps({'compile': command, 'link': link, 'run': run,
    'exit': result.returncode}, indent=2) + '\n')
print(result.stdout + result.stderr)
raise SystemExit(result.returncode)
