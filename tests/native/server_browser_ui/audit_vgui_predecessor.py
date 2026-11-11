"""Prove unchanged actual dependency closures before using admitted headless VGUI predecessor images."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--repo', type=Path, required=True)
parser.add_argument('--predecessor', type=Path, required=True)
parser.add_argument('--packet', type=Path, required=True)
parser.add_argument('--profile', choices=['release', 'debug'], required=True)
args = parser.parse_args()
repo = args.repo.resolve()
base = args.predecessor.resolve()
packet = args.packet.resolve()
packet.mkdir(exist_ok=False, parents=True)
manifest = json.loads((base / 'source-manifest.json').read_text())
source = Path(manifest['source'])
cache = base / args.profile
entries = json.loads((cache / 'compile_commands.json').read_text())
selected = [entry for entry in entries if any('/' + target + '.dir/' in entry.get('output', '')
            for target in ['vgui2', 'filesystem_stdio', 'cse_font_registration'])]
assert selected
result = subprocess.run(['ninja', '-t', 'deps'] + [str(Path(entry['output']).relative_to(cache)) if Path(entry['output']).is_absolute() else entry['output']
                                        for entry in selected], cwd=cache,
                        capture_output=True, text=True, check=True)
(packet / 'actual-dependencies.txt').write_text(result.stdout)
records = {}
for line in result.stdout.splitlines():
    if not line.startswith('    '):
        continue
    path = Path(line.strip())
    if not path.is_absolute():
        path = cache / path
    if not path.is_file():
        continue
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if path.is_relative_to(source):
        relative = path.relative_to(source).as_posix()
        current = repo / relative
        records[str(path)] = dict(relative=relative, recorded=manifest['files'].get(relative),
            predecessor=digest, current=hashlib.sha256(current.read_bytes()).hexdigest() if current.is_file() else None)
    else:
        records[str(path)] = dict(external=True, sha256=digest)
drift = [record for record in records.values() if not record.get('external') and
         not record['recorded'] == record['predecessor'] == record['current']]
(packet / 'source-closure.json').write_text(json.dumps(dict(records=records, drift=drift,
    commands=selected, manifestSha256=hashlib.sha256((base / 'source-manifest.json').read_bytes()).hexdigest()),
    indent=2) + '\n')
print('objects', len(selected), 'dependency files', len(records), 'changed inputs', len(drift))
for record in drift:
    print(record['relative'])
raise SystemExit(bool(drift))
