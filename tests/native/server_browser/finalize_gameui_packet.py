"""Rebuild selected model/config TUs and all changed-header consumers before full owning links."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import shlex
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--repo', type=Path, required=True)
parser.add_argument('--predecessor', type=Path, required=True)
parser.add_argument('--packet', type=Path, required=True)
parser.add_argument('--owner', choices=['ServerBrowserModel', 'ServerBrowserConfig'], action='append')
parser.add_argument('--changed-header', type=Path, action='append', default=[])
args = parser.parse_args()
repo = args.repo.resolve()
base = args.predecessor.resolve()
packet = args.packet.resolve()
packet.mkdir(parents=True, exist_ok=False)
receipt_path = base / 'receipt.json'
receipt = json.loads(receipt_path.read_text())
owners = args.owner or ['ServerBrowserModel']
assert len(owners) == len(set(owners))
changed_headers = {(repo / path).resolve() for path in args.changed_header}
changed_sources = {repo / 'GoldSrc-SDK/GameUI/native' / (owner + '.cpp') for owner in owners}
changed = changed_sources | changed_headers

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def dependencies(depfile):
    return {Path(name).resolve() for name in shlex.split(depfile.read_text().replace('\\\n', ' '))[1:]}

for name, sha in receipt['dependencies'].items():
    if Path(name) not in changed:
        assert digest(Path(name)) == sha, name
assert all(str(path) in receipt['dependencies'] for path in changed)
for name, sha in receipt['outputs'].items():
    assert digest(base / name) == sha, name
unchanged_objects = dict(receipt.get('unchangedOwningObjects', {}))
unchanged_objects.update({str(base / name): sha for name, sha in receipt['outputs'].items()
                          if name.endswith('.o')})
consumer_audit = {}
for name, sha in unchanged_objects.items():
    obj = Path(name)
    assert digest(obj) == sha, name
    depfile = obj.with_suffix('.d')
    assert depfile.is_file(), str(depfile)
    consumer_audit[name] = dict(objectSha256=sha, depfile=str(depfile), depfileSha256=digest(depfile),
        changedHeaders=sorted(str(path) for path in dependencies(depfile) & changed_headers))
for name, sha in receipt['linkInputs'].items():
    assert digest(Path(name)) == sha, name
commands = []

def run(label, argv, cwd):
    result = subprocess.run(argv, cwd=cwd, capture_output=True, text=True)
    (packet / (label + '.log')).write_text(result.stdout + result.stderr)
    commands.append(dict(label=label, argv=argv, cwd=cwd, exit=result.returncode,
                         warnings=result.stderr.count('warning:')))
    (packet / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
    assert result.returncode == 0, label

# Resolve the most recent compile recipe for each selected owner through immutable predecessor receipts.
compile_entries = {}
for owner in owners:
    label = 'compile-' + owner
    recipe = receipt
    while not any(command['label'] == label for command in recipe['commands']):
        parent = Path(recipe['predecessor'])
        assert digest(parent) == recipe['predecessorSha256'], str(parent)
        recipe = json.loads(parent.read_text())
    compile_entries[owner] = next(command for command in recipe['commands'] if command['label'] == label)
replacements = {entry['argv'][entry['argv'].index('-o') + 1]: str(packet / (owner + '.o'))
                for owner, entry in compile_entries.items()}
affected = {name for name, row in consumer_audit.items() if row['changedHeaders']}
assert affected <= replacements.keys(), 'Unrebuilt changed-header consumers: ' + str(affected - replacements.keys())
(packet / 'consumer-audit.json').write_text(json.dumps(consumer_audit, indent=2) + '\n')
changed_pins = {str(path): digest(path) for path in changed}
for owner, entry in compile_entries.items():
    argv = entry['argv'].copy()
    argv[argv.index('-o') + 1] = str(packet / (owner + '.o'))
    argv[argv.index('-MF') + 1] = str(packet / (owner + '.d'))
    run('compile-' + owner, argv, entry['cwd'])
    for path in dependencies(packet / (owner + '.d')):
        assert str(path) in receipt['dependencies'], 'New dependency requires owning closure rebuild: ' + str(path)
        expected = changed_pins.get(str(path), receipt['dependencies'][str(path)])
        assert digest(path) == expected, str(path)
link_entry = next(command for command in receipt['commands'] if command['label'] == 'full-link-gameui')
assert replacements.keys() <= set(link_entry['argv'])
argv = [replacements.get(value, value) for value in link_entry['argv']]
argv[argv.index('-o') + 1] = str(packet / 'GameUI.dylib')
run('full-link-gameui', argv, link_entry['cwd'])
run('sign-gameui', ['codesign', '--force', '--sign', '-', str(packet / 'GameUI.dylib')], str(packet))
run('image-policy', ['otool', '-L', '-l', str(packet / 'GameUI.dylib')], str(packet))
run('image-symbols', ['nm', '-gU', str(packet / 'GameUI.dylib')], str(packet))
pins = dict(receipt['dependencies'])
pins.update(changed_pins)
for name, sha in pins.items():
    path = Path(name)
    assert digest(path) == sha, name
    if path.is_relative_to(repo):
        target = packet / 'source' / path.relative_to(repo)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, target)
shutil.copyfile(Path(__file__), packet / 'finalize_gameui_packet.py')
(packet / 'receipt.json').write_text(json.dumps(dict(config=receipt['config'],
    toolSha256=digest(Path(__file__)), predecessor=str(receipt_path), predecessorSha256=digest(receipt_path),
    changedSources=sorted(str(path) for path in changed_sources), changedHeaders=sorted(str(path) for path in changed_headers),
    affectedConsumers=sorted(affected), consumerAuditSha256=digest(packet / 'consumer-audit.json'),
    dependencies=pins, commands=commands, unchangedOwningObjects={name: sha
        for name, sha in unchanged_objects.items() if name not in replacements},
    linkInputs=receipt['linkInputs'], outputs={name: digest(packet / name)
        for name in [owner + '.o' for owner in owners] + ['GameUI.dylib']},
    warnings=sum(command['warnings'] for command in commands),
    inheritedWarnings=receipt.get('inheritedWarnings', receipt['warnings']), liveRun=False), indent=2) + '\n')
print(receipt['config'], ', '.join(owners), 'compiled;', len(unchanged_objects) - len(replacements),
      'unchanged owning TUs verified;', len(affected), 'changed-header consumers rebuilt; full link passed')
