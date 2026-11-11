"""Freshly compile every GameUI TU under retained owning flags and full-link a frozen packet.

Read-only cache inputs; no configuration, dependency build, app mutation or runtime invocation.
At most two compiler processes. New manifest-selected native sources inherit the target's exact policy.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import shlex
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--repo', type=Path, required=True)
parser.add_argument('--cache', type=Path, required=True)
parser.add_argument('--packet', type=Path, required=True)
parser.add_argument('--config', choices=['Release', 'Debug'], required=True)
parser.add_argument('--link-images', type=Path, required=True)
args = parser.parse_args()
repo = args.repo.resolve()
cache = args.cache.resolve()
packet = args.packet.resolve()
packet.mkdir(parents=True, exist_ok=False)
(packet / 'objects').mkdir()
entries = json.loads((cache / 'compile_commands.json').read_text())
entries = [entry for entry in entries if '/gameui.dir/' in entry['output']]
assert entries
original_root = Path(next(entry['file'] for entry in entries if '/GameUI/GameUI_Interface.cpp' in entry['file']))
original_root = original_root.parents[2]
commands = []

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def remap(value):
    return value.replace(str(original_root / 'cmake/../GoldSrc-SDK'), str(repo / 'GoldSrc-SDK')).replace(
        str(original_root / 'GoldSrc-SDK'), str(repo / 'GoldSrc-SDK'))

def run(label, argv, cwd=cache):
    result = subprocess.run([str(value) for value in argv], cwd=cwd, capture_output=True, text=True)
    (packet / (label + '.log')).write_text(result.stdout + result.stderr)
    return dict(label=label, argv=[str(value) for value in argv], cwd=str(cwd), exit=result.returncode,
                warnings=result.stderr.count('warning:'), log=label + '.log')

def checked(label, argv, cwd=cache):
    result = run(label, argv, cwd)
    commands.append(result)
    (packet / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
    if result['exit']:
        raise RuntimeError(label)

recipe_result = subprocess.run(['ninja', '-t', 'commands',
    f'runtime/{args.config}/valve/cl_dlls/GameUI.dylib'], cwd=cache, capture_output=True, text=True, check=True)
(packet / 'retained-link-recipe.txt').write_text(recipe_result.stdout.splitlines()[-1] + '\n')
recipe = shlex.split(recipe_result.stdout.splitlines()[-1])
assert recipe[:2] == [':', '&&'] and recipe[-2:] == ['&&', ':']
link = recipe[2:-2]
assert '&&' not in link
# Freeze policy and source inputs before compiling. Only the actual used dependency closure is copied below.
initial = {path.resolve(): digest(path) for path in (repo / 'GoldSrc-SDK').rglob('*')
           if path.is_file() and path.suffix in ['.h', '.hpp', '.inc', '.cpp', '.c']}
(packet / 'precompile-pins.json').write_text(json.dumps({str(k): v for k, v in initial.items()}, indent=2) + '\n')
object_map = {}
jobs = []
for index, entry in enumerate(entries):
    argv = [remap(value) for value in shlex.split(entry['command'])]
    old = argv[argv.index('-o') + 1]
    output = packet / 'objects' / f'{index:03d}-{Path(entry["file"]).name}.o'
    argv[argv.index('-o') + 1] = str(output)
    argv += ['-MD', '-MF', str(output.with_suffix('.d'))]
    object_map[old] = str(output)
    jobs.append((f'compile-{index:03d}', argv))
owner = next(entry for entry in entries if entry['file'].endswith('/GameUI/GameUI_Interface.cpp'))
for name in ['ServerBrowserModel', 'ServerBrowserConfig', 'ServerBrowserPresentation', 'ServerBrowserConnection',
             'ServerBrowserDetails', 'ServerBrowserDialog', 'ServerBrowserModule']:
    argv = [remap(value) for value in shlex.split(owner['command'])]
    output = packet / 'objects' / (name + '.o')
    argv[argv.index('-o') + 1] = str(output)
    argv[argv.index('-c') + 1] = str(repo / 'GoldSrc-SDK/GameUI/native' / (name + '.cpp'))
    argv += ['-MD', '-MF', str(output.with_suffix('.d'))]
    jobs.append(('compile-' + name, argv))
with ThreadPoolExecutor(max_workers=2) as pool:
    for result in pool.map(lambda job: run(*job), jobs):
        commands.append(result)
        (packet / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
        print(result['label'], result['exit'], flush=True)
assert all(command['exit'] == 0 for command in commands), 'Owning compile failed; retained policies unchanged'
link = [object_map.get(value, value) for value in link]
link[link.index('-o') + 1] = str(packet / 'GameUI.dylib')
link += [str(packet / 'objects' / (name + '.o')) for name in ['ServerBrowserModel', 'ServerBrowserConfig', 'ServerBrowserPresentation', 'ServerBrowserConnection',
             'ServerBrowserDetails', 'ServerBrowserDialog', 'ServerBrowserModule']]
approved = json.loads(args.link_images.read_text())
tier0 = Path(approved[args.config]['approved_tier0'])
sdl = Path(approved[args.config.lower()]['approved_sdl'])
assert digest(tier0) == approved[args.config]['sha256']
assert digest(sdl) == approved[args.config.lower()]['sha256']
for index, value in enumerate(link):
    if value == f'runtime/{args.config}/tier0.dylib':
        link[index] = str(tier0)
    elif value == f'dependencies/SDL3/lib/{args.config}/libSDL3.0.dylib':
        link[index] = str(sdl)
    elif value == '-Wl,-rpath,' + str(cache / f'runtime/{args.config}'):
        link[index] = '-Wl,-rpath,' + str(tier0.parent)
    elif value == '-Wl,-rpath,' + str(cache / f'dependencies/SDL3/lib/{args.config}'):
        link[index] = '-Wl,-rpath,' + str(sdl.parent)
inputs = {str((cache / value).resolve()): digest((cache / value).resolve()) for value in link
          if value.endswith(('.a', '.dylib')) and value != str(packet / 'GameUI.dylib')
          and (cache / value).is_file()}
assert set(inputs) == {str(tier0), str(sdl)}, inputs
shutil.copyfile(args.link_images, packet / 'approved-link-images.json')
checked('full-link-gameui', link)
checked('sign-gameui', ['codesign', '--force', '--sign', '-', packet / 'GameUI.dylib'])
checked('image-policy', ['otool', '-L', '-l', packet / 'GameUI.dylib'])
checked('image-symbols', ['nm', '-gU', packet / 'GameUI.dylib'])
dependencies = set()
for depfile in (packet / 'objects').glob('*.d'):
    tokens = shlex.split(depfile.read_text().replace('\\\n', ' '))
    for value in tokens[1:]:
        path = Path(value)
        if not path.is_absolute():
            path = cache / path
        if path.is_file():
            dependencies.add(path.resolve())
pins = {}
for path in sorted(dependencies):
    sha = digest(path)
    if path in initial:
        assert initial[path] == sha, f'Input changed while compiling: {path}'
    pins[str(path)] = sha
    if path.is_relative_to(repo / 'GoldSrc-SDK'):
        target = packet / 'source' / path.relative_to(repo)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, target)
for path, sha in inputs.items():
    assert digest(Path(path)) == sha
for path in ['GoldSrc-SDK/projects/vs2026/GameUI.vcxproj', 'cmake/targets/gameui/Sources.cmake',
             'tools/translate_msbuild.py', 'tests/native/server_browser_ui/build_gameui_packet.py']:
    target = packet / 'source' / path
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(repo / path, target)
    pins[str(repo / path)] = digest(repo / path)
receipt = dict(config=args.config, owningFlags='exact retained GameUI flags; no added suppressions',
    owningTranslationUnits=len(jobs), commands=commands, dependencies=pins, linkInputs=inputs,
    outputs={str(path.relative_to(packet)): digest(path) for path in packet.rglob('*')
             if path.is_file() and path.suffix in ['.o', '.dylib']}, cache=str(cache),
    warnings=sum(command['warnings'] for command in commands), liveRun=False)
(packet / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
print(f'{args.config}: {len(jobs)} fresh owning TUs, full GameUI link, {len(pins)} dependency pins')
