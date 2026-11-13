#!/usr/bin/env python3
"""Link provider-only assertions with the admitted owning-control fixture packet."""
import argparse
import json
from pathlib import Path
import subprocess
import shutil
from concurrent.futures import ThreadPoolExecutor

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
shutil.copytree(packet / 'extracted', out / 'extracted', dirs_exist_ok=True)
forward = out / 'extracted/geometry_forward.inc'
text = forward.read_text().replace('return real_->SetTitle(panel, title);',
    'captions[panel] = title ? title : L""; return real_->SetTitle(panel, title);')
forward.write_text(text)
entries = json.loads((packet / 'commands.json').read_text())
driver = next(entry for entry in entries if entry['source'].endswith('/runtime_tests.cpp'))
command = driver['argv'][:]
command += ['-fno-access-control', '-I' + str(out / 'extracted')]
command = [arg.replace(str(packet / 'extracted'), str(out / 'extracted')) for arg in command]
command[command.index('-c') + 1] = str(root / 'tests/native/live_localization_vgui/controls_tests.cpp')
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
selected = [entry for entry in entries if '/vgui2/controls/' in entry['source']]
def rebuild(entry):
    arguments = entry['argv'][:]
    target = out / Path(entry['object']).name
    arguments[arguments.index('-o') + 1] = str(target)
    if '-MF' in arguments:
        arguments[arguments.index('-MF') + 1] = str(target.with_suffix('.d'))
    result = subprocess.run(arguments, cwd=entry['cwd'], text=True, capture_output=True)
    (out / (target.name + '.log')).write_text(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(result.stderr)
    return entry['object'], str(target)
with ThreadPoolExecutor(max_workers=4) as pool:
    replacements = dict(pool.map(rebuild, selected))
link = [replacements.get(argument, argument) for argument in link]
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
