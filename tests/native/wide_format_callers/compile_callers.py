"""Compile frozen copies of whole caller TUs under actual selected Mac command rows."""
import argparse
import concurrent.futures
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--artifacts', type=Path, required=True)
p.add_argument('--before', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=False)
root = Path(__file__).resolve().parents[3]
names = [x for x in json.loads((a.before / 'before-hashes.json').read_text()) if x.endswith('.cpp')]
names.append('src/cstrike/public/KeyValues.cpp')
work = []
missing = []
for profile in ('release', 'debug'):
    database = a.artifacts / ('macos-runtime-policy-' + profile) / 'compile_commands.json'
    rows = json.loads(database.read_text())
    for name in names:
        original = next((r for r in rows if r['file'] == str(root / name)), None)
        if not original:
            missing.append({'profile': profile, 'file': name})
            continue
        out = a.output / profile / name.replace('/', '_')
        out.mkdir(parents=True)
        copied = out / Path(name).name
        copied.write_bytes((root / name).read_bytes())
        command = shlex.split(original['command'])
        command[command.index('-c') + 1] = str(copied)
        command[command.index('-o') + 1] = str(out / 'caller.o')
        command.extend(['-iquote', str((root / name).parent), '-MD', '-MF', str(out / 'caller.d')])
        work.append((profile, name, original, out, copied, command))

def run(item):
    profile, name, original, out, copied, command = item
    with (out / 'stdout.log').open('wb') as stdout, (out / 'stderr.log').open('wb') as stderr:
        result = subprocess.run(command, cwd=original['directory'], stdout=stdout, stderr=stderr, timeout=90)
    row = {'profile': profile, 'file': name, 'original': original, 'command': command,
           'source_sha256': hashlib.sha256(copied.read_bytes()).hexdigest(), 'exit': result.returncode}
    if result.returncode == 0:
        row['object_sha256'] = hashlib.sha256((out / 'caller.o').read_bytes()).hexdigest()
    return row
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    results = list(pool.map(run, work))
(a.output / 'result.json').write_text(json.dumps({'scope': 'One actual selected owner per source/profile; whole TU exact frozen source bytes, original quote-include directory restored; compile only, no source inventory/link acceptance', 'cases': results, 'not_in_selected_database': missing}, indent=2) + '\n')
for row in results:
    print(row['profile'], row['file'], row['exit'])
print('not selected:', missing)
raise SystemExit(int(any(x['exit'] for x in results)))
