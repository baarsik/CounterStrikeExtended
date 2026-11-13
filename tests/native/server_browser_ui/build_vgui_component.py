"""Fresh actual shared controls + browser component; retained owning flags, admitted dummy VGUI images."""
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
parser.add_argument('--packet', type=Path, required=True)
parser.add_argument('--profile', choices=['release', 'debug'], required=True)
parser.add_argument('--sanitizers', action='store_true')
parser.add_argument('--recipe-cache', type=Path, help='Current owning CMake build with compile_commands.json')
parser.add_argument('--sdl-library', type=Path, help='SDL library matching the owning packet')
parser.add_argument('--driver', choices=['vgui', 'module', 'translation_fit'], default='vgui')
args = parser.parse_args()
repo = args.repo.resolve()
packet = args.packet.resolve()
packet.mkdir(exist_ok=False, parents=True)
subprocess.run(['python3', str(repo / 'tools/build_localization.py'),
    '--source', str(repo / 'src/deploy/localization'), '--output', str(packet / 'resources'),
    '--fallback-header', str(packet / 'extracted/ServerBrowserEnglish.generated.inc')], check=True)
if args.driver == 'translation_fit':
    shutil.copytree(repo / 'src/deploy/platform', packet / 'resources/platform', dirs_exist_ok=True)
cache = Path('/Users/mmalakhouski/Library/Caches/CounterStrikeExtended/CodexQualification/gameplay-mouse-20261110/final') / args.profile
cache = args.recipe_cache.resolve() if args.recipe_cache else cache
entries = json.loads((cache / 'compile_commands.json').read_text())
entries = [entry for entry in entries if '/gameui.dir/' in entry.get('output', '')]
owner = next(entry for entry in entries if entry['file'].endswith('/GameUI/GameUI_Interface.cpp'))
source_prefix = owner['file'].split('/src/')[0]
extra = ['engine/controls.cpp', 'engine/interface.cpp', 'engine/mem.c', 'engine/KeyValues.cpp',
         'engine/utlbuffer.cpp', 'engine/utlsymbol.cpp', 'engine/MemPool.cpp',
         'common/TextEntryClipboard.cpp', 'common/legacy_wide_format.cpp',
         'public/characterset.cpp', 'vgui2/src/public/filesystem_helpers.cpp']
selected = [entry for entry in entries if '/vgui2/controls/' in entry['file'] or
            any(entry['file'].endswith('/' + name) for name in extra)]
assert all(any(entry['file'].endswith('/' + name) for entry in selected)
           for name in ['Frame.cpp', 'ListPanel.cpp', 'TextEntry.cpp', 'KeyValues.cpp', 'controls.cpp'])
subprocess.run(['python3', str(repo / 'tests/native/server_browser_ui/extract_owners.py'),
    '--repo', str(repo), '--output', str(packet / 'extracted')], check=True)
subprocess.run(['python3', str(repo / 'tests/native/server_browser_ui/extract_geometry.py'),
    '--repo', str(repo), '--output', str(packet / 'extracted')], check=True)
new = ['ServerBrowserModel', 'ServerBrowserConfig', 'ServerBrowserPresentation', 'ServerBrowserConnection',
       'ServerBrowserDetails', 'ServerBrowserDialog']
if args.driver == 'module':
    new.append('ServerBrowserModule')
for name in new:
    selected.append(dict(owner, file=str(repo / 'src/GameUI/native' / (name + '.cpp'))))
selected.append(dict(owner, file=str(repo / ('tests/native/server_browser_ui/' + args.driver + '_tests.cpp'))))

def run(pair):
    index, entry = pair
    argv = [value.replace(source_prefix + '/src', str(repo / 'src'))
            for value in shlex.split(entry['command'])]
    source = entry['file'].replace(source_prefix + '/src', str(repo / 'src'))
    output = packet / (f'{index:03d}-' + Path(source).name + '.o')
    argv[argv.index('-c') + 1] = source
    argv[argv.index('-o') + 1] = str(output)
    argv += ['-MD', '-MF', str(output.with_suffix('.d')), '-I' + str(packet / 'extracted'),
             '-I' + str(repo / 'src/GameUI/native')]
    if args.sanitizers:
        argv += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    before = hashlib.sha256(Path(source).read_bytes()).hexdigest()
    result = subprocess.run(argv, cwd=cache, capture_output=True, text=True)
    (packet / (output.name + '.log')).write_text(result.stdout + result.stderr)
    assert before == hashlib.sha256(Path(source).read_bytes()).hexdigest()
    return dict(argv=argv, cwd=str(cache), exit=result.returncode, object=str(output), source=source, sourceSha256=before)
with ThreadPoolExecutor(max_workers=2) as workers:
    results = list(workers.map(run, enumerate(selected)))
(packet / 'commands.json').write_text(json.dumps(results, indent=2) + '\n')
failed = [result for result in results if result['exit']]
for result in failed:
    print('FAILED', result['source'])
if failed:
    raise SystemExit(1)
sdl = repo / ('build/gameplay-shortcuts-fix-round1-20261110/provider-' + args.profile) / 'libSDL3.0.dylib'
sdl = args.sdl_library.resolve() if args.sdl_library else sdl
link = ['/usr/bin/c++', '-arch', 'arm64', '-mmacosx-version-min=14.0', '-o', str(packet / 'vgui_tests')]
link += [result['object'] for result in results]
link += [str(sdl), '-Wl,-rpath,' + str(sdl.parent), '-framework', 'CoreFoundation', '-framework', 'ApplicationServices']
if args.sanitizers:
    link += ['-fsanitize=address,undefined']
result = subprocess.run(link, capture_output=True, text=True)
(packet / 'link.log').write_text(result.stdout + result.stderr)
(packet / 'link-command.json').write_text(json.dumps(dict(argv=link, exit=result.returncode), indent=2) + '\n')
print('compiled', len(results), 'link', result.returncode)
raise SystemExit(result.returncode)
