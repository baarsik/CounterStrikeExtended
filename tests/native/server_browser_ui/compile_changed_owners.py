"""Compile real changed engine owners with retained HW/SW recipes; never mutate a predecessor cache."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--repo', type=Path, required=True)
parser.add_argument('--predecessor', type=Path, required=True)
parser.add_argument('--packet', type=Path, required=True)
args = parser.parse_args()
repo = args.repo.resolve()
packet = args.packet.resolve()
packet.mkdir(exist_ok=False, parents=True)
predecessor = json.loads(args.predecessor.read_text())
cache = Path(predecessor['baseBuild'])
entries = json.loads((cache / 'compile_commands.json').read_text())
names = ['sys_dll2.cpp', 'sys_engine.cpp', 'host.cpp', 'vgui_int.cpp', 'vgui_intwrap.cpp',
         'vgui_intwrap2.cpp', 'BaseUI_Interface.cpp', 'filesystem.cpp']
selected = [entry for entry in entries if any(entry['file'].endswith('/' + name) for name in names)
            and any('/' + target + '.dir/' in entry.get('output', '') for target in ['hw_engine', 'swds'])]
assert len(selected) == 2 * len(names), len(selected)
source_prefix = next(entry['file'].split('/GoldSrc-SDK/')[0] for entry in selected)

def run(entry):
    target = 'hw_engine' if '/hw_engine.dir/' in entry['output'] else 'swds'
    name = target + '-' + Path(entry['file']).name
    argv = [value.replace(source_prefix + '/GoldSrc-SDK', str(repo / 'GoldSrc-SDK'))
            for value in shlex.split(entry['command'])]
    output = packet / (name + '.o')
    argv[argv.index('-o') + 1] = str(output)
    argv += ['-MD', '-MF', str(output.with_suffix('.d'))]
    source = Path(argv[argv.index('-c') + 1])
    before = hashlib.sha256(source.read_bytes()).hexdigest()
    result = subprocess.run(argv, cwd=cache, capture_output=True, text=True)
    (packet / (name + '.log')).write_text(result.stdout + result.stderr)
    assert before == hashlib.sha256(source.read_bytes()).hexdigest(), 'source mutated during compile'
    return dict(name=name, argv=argv, cwd=str(cache), source=str(source), sourceSha256=before,
                exit=result.returncode, log=name + '.log')

with ThreadPoolExecutor(max_workers=2) as workers:
    results = list(workers.map(run, selected))
(packet / 'commands.json').write_text(json.dumps(results, indent=2) + '\n')
for result in results:
    print(result['name'], result['exit'])
raise SystemExit(0 if all(result['exit'] == 0 for result in results) else 1)
