"""Build the actual native VGUI2 owner with retained flags and current-source inputs.

This intentionally compiles every owning TU: the predecessor cache does not seal
individual objects strongly enough to claim selective object reuse.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--repo', type=Path, required=True)
parser.add_argument('--packet', type=Path, required=True)
parser.add_argument('--profile', choices=['release', 'debug'], required=True)
parser.add_argument('--sanitizers', action='store_true')
args = parser.parse_args()
repo = args.repo.resolve()
packet = args.packet.resolve()
packet.mkdir(exist_ok=False, parents=True)
predecessor = Path('/Users/mmalakhouski/Library/Caches/CounterStrikeExtended/CodexQualification/gameplay-mouse-20261110/final')
cache = predecessor / args.profile
entries = json.loads((cache / 'compile_commands.json').read_text())
entries = [entry for entry in entries if '/vgui2.dir/' in entry.get('output', '')]
assert len(entries) == 38, len(entries)
assert any(entry['file'].endswith('/InputWin32.cpp') for entry in entries)
source_prefix = entries[0]['file'].split('/GoldSrc-SDK/')[0]
recipe = subprocess.run(['ninja', '-t', 'commands', 'vgui2'], cwd=cache, capture_output=True,
                        text=True, check=True).stdout
(packet / 'retained-recipes.txt').write_text(recipe)

def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def compile_one(pair):
    index, entry = pair
    argv = [value.replace(source_prefix + '/GoldSrc-SDK', str(repo / 'GoldSrc-SDK'))
            for value in shlex.split(entry['command'])]
    source = Path(entry['file'].replace(source_prefix + '/GoldSrc-SDK', str(repo / 'GoldSrc-SDK')))
    output = packet / (f'{index:03d}-' + source.name + '.o')
    argv[argv.index('-c') + 1] = str(source)
    argv[argv.index('-o') + 1] = str(output)
    argv += ['-MD', '-MF', str(output.with_suffix('.d'))]
    if args.sanitizers:
        argv += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    before = digest(source)
    result = subprocess.run(argv, cwd=cache, capture_output=True, text=True)
    (packet / (output.name + '.log')).write_text(result.stdout + result.stderr)
    assert digest(source) == before
    return dict(argv=argv, retainedArgv=shlex.split(entry['command']), cwd=str(cache), exit=result.returncode,
                source=str(source), sourceSha256=before, object=str(output))

with ThreadPoolExecutor(max_workers=2) as workers:
    commands = list(workers.map(compile_one, enumerate(entries)))
(packet / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
assert not any(command['exit'] for command in commands), [c['source'] for c in commands if c['exit']]
link_line = next(line for line in recipe.splitlines() if ' -o runtime/' in line and '/vgui2.dylib ' in line)
link = shlex.split(link_line.split('&&')[1].strip())
config = args.profile.capitalize()
sdl = repo / ('build/gameplay-shortcuts-fix-round1-20261110/provider-' + args.profile) / 'libSDL3.0.dylib'
frozen = predecessor / 'controller-frozen-runtime' / config
objects = {str(Path(entry['output']).relative_to(cache)): command['object']
           for entry, command in zip(entries, commands)}
link = [objects.get(value, value) for value in link]
link[link.index('-o') + 1] = str(packet / 'vgui2.dylib')
if args.sanitizers:
    link += ['-fsanitize=address,undefined']
link_inputs = {}
for index, value in enumerate(link):
    if value.endswith('/libSDL3.0.dylib'):
        link[index] = str(sdl)
    elif value.endswith('/libcse_font_registration.dylib'):
        link[index] = str(frozen / 'libcse_font_registration.dylib')
    elif value.startswith('-Wl,-rpath,') and ('runtime/' in value or 'dependencies/SDL3' in value):
        link[index] = '-Wl,-rpath,' + str(sdl.parent if 'SDL3' in value else frozen)
for value in link:
    path = Path(value)
    if not value.startswith('-') and path.suffix in {'.a', '.dylib'}:
        path = path if path.is_absolute() else cache / path
        if path.is_file():
            link_inputs[str(path)] = digest(path)
result = subprocess.run(link, cwd=cache, capture_output=True, text=True)
(packet / 'link.log').write_text(result.stdout + result.stderr)
(packet / 'link-command.json').write_text(json.dumps(dict(argv=link, cwd=str(cache), exit=result.returncode), indent=2) + '\n')
assert result.returncode == 0, result.stderr
subprocess.run(['codesign', '--force', '--sign', '-', str(packet / 'vgui2.dylib')], check=True,
               capture_output=True)
dependencies = {}
for command in commands:
    dep = Path(command['object']).with_suffix('.d')
    for value in shlex.split(dep.read_text().replace('\\\n', ' '))[1:]:
        path = Path(value)
        path = (path if path.is_absolute() else cache / path).resolve()
        if path.is_file():
            dependencies[str(path)] = digest(path)
    command['objectSha256'] = digest(command['object'])
for path, expected in link_inputs.items():
    assert digest(path) == expected
(packet / 'receipt.json').write_text(json.dumps(dict(profile=args.profile, commands=commands,
    dependencies=dependencies, linkInputs=link_inputs, link=link,
    outputSha256=digest(packet / 'vgui2.dylib'), owningTranslationUnits=len(commands),
    freshlyCompiled=len(commands), reused=0, sourceSelection='actual retained vgui2 target', sanitizers=args.sanitizers, liveRun=False), indent=2) + '\n')
print('compiled', len(commands), 'linked current VGUI2', args.profile)
