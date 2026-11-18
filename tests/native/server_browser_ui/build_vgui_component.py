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
parser.add_argument('--driver', choices=['vgui', 'module', 'translation_fit', 'interface_options', 'options_voice', 'live_localization', 'label_layout', 'zbot_layout'], default='vgui')
parser.add_argument('--label-variant', choices=('shared', 'cs'), default='shared',
                    help='Compile the actual shared or CS Label owner with real control dependencies')
args = parser.parse_args()
repo = args.repo.resolve()
packet = args.packet.resolve()
packet.mkdir(exist_ok=False, parents=True)
subprocess.run(['python3', str(repo / 'tools/build_localization.py'),
    '--source', str(repo / 'src/deploy/localization'), '--output', str(packet / 'resources'),
    '--fallback-header', str(packet / 'extracted/ServerBrowserEnglish.generated.inc'),
    '--language-header', str(packet / 'extracted/InterfaceLanguages.generated.inc')], check=True)
if args.driver in ('translation_fit', 'interface_options', 'options_voice'):
    shutil.copytree(repo / 'src/deploy/platform', packet / 'resources/platform', dirs_exist_ok=True)
cache = Path('/Users/mmalakhouski/Library/Caches/CounterStrikeExtended/CodexQualification/gameplay-mouse-20261110/final') / args.profile
cache = args.recipe_cache.resolve() if args.recipe_cache else cache
all_entries = json.loads((cache / 'compile_commands.json').read_text())
entries = all_entries
entries = [entry for entry in entries if '/gameui.dir/' in entry.get('output', '')]
owner = next(entry for entry in entries if entry['file'].endswith('/GameUI/GameUI_Interface.cpp'))
source_prefix = owner['file'].split('/src/')[0]
extra = ['engine/controls.cpp', 'engine/interface.cpp', 'engine/mem.c', 'engine/KeyValues.cpp',
         'engine/utlbuffer.cpp', 'engine/utlsymbol.cpp', 'engine/MemPool.cpp',
         'common/TextEntryClipboard.cpp', 'common/legacy_wide_format.cpp',
         'public/characterset.cpp', 'vgui2/src/public/filesystem_helpers.cpp']
selected = [entry for entry in entries if '/vgui2/controls/' in entry['file'] or
            any(entry['file'].endswith('/' + name) for name in extra)]
if args.label_variant == 'cs':
    cs_label = next(entry for entry in all_entries if '/cs_client.dir/' in entry.get('output', '')
                    and entry['file'].endswith('/cstrike/vgui2/controls/Label.cpp'))
    selected = [cs_label if entry['file'].endswith('/src/vgui2/controls/Label.cpp') else entry
                for entry in selected]
assert all(any(entry['file'].endswith('/' + name) for entry in selected)
           for name in ['Frame.cpp', 'ListPanel.cpp', 'TextEntry.cpp', 'KeyValues.cpp', 'controls.cpp'])
subprocess.run(['python3', str(repo / 'tests/native/server_browser_ui/extract_owners.py'),
    '--repo', str(repo), '--output', str(packet / 'extracted')], check=True)
subprocess.run(['python3', str(repo / 'tests/native/server_browser_ui/extract_geometry.py'),
    '--repo', str(repo), '--output', str(packet / 'extracted')], check=True)
new = ['ServerBrowserModel', 'ServerBrowserConfig', 'ServerBrowserPresentation', 'ServerBrowserConnection',
       'ServerBrowserDetails', 'ServerBrowserMasterserverQuery', 'ServerBrowserSettingsDialog', 'ServerBrowserDialog']
if args.driver == 'module':
    new.append('ServerBrowserModule')
for name in new:
    selected.append(dict(owner, file=str(repo / 'src/GameUI/native' / (name + '.cpp'))))
if args.driver == 'interface_options':
    selected.append(dict(owner, file=str(repo / 'src/GameUI/OptionsSubInterface.cpp')))
    selected.append(dict(owner, file=str(repo / 'src/GameUI/HudRadarDialog.cpp')))
    selected.append(dict(owner, file=str(repo / 'src/GameUI/CvarToggleCheckButton.cpp')))
    keyboard_path = repo / 'src/GameUI/OptionsSubKeyboard.cpp'
    keyboard_source = keyboard_path.read_text(encoding='cp1252')
    keyboard_start = keyboard_source.index('void COptionsSubKeyboard::OnKeyCodePressed(')
    keyboard_brace = keyboard_source.index('{', keyboard_start)
    keyboard_depth = 1
    keyboard_end = keyboard_brace + 1
    while keyboard_depth:
        keyboard_depth += (keyboard_source[keyboard_end] == '{') - (keyboard_source[keyboard_end] == '}')
        keyboard_end += 1
    (packet / 'extracted/keyboard_pressed_owner.inc').write_text(keyboard_source[keyboard_start:keyboard_end] + '\n')
    (packet / 'extracted/keyboard-receipt.json').write_text(json.dumps({
        'source': str(keyboard_path.relative_to(repo)),
        'sourceSha256': hashlib.sha256(keyboard_path.read_bytes()).hexdigest()}, indent=2) + '\n')
    reset_receipt = {}
    reset_bodies = []
    for page in ('Video', 'Advanced'):
        reset_path = repo / f'src/GameUI/OptionsSub{page}.cpp'
        reset_source = reset_path.read_text(encoding='cp1252')
        reset_receipt[str(reset_path.relative_to(repo))] = hashlib.sha256(reset_path.read_bytes()).hexdigest()
        methods = ['OnDataChanged()', 'OnButtonChecked(']
        if page == 'Video':
            methods.append('PrepareResolutionList(')
        for method in methods:
            start = reset_source.index(f'void COptionsSub{page}::{method}')
            brace = reset_source.index('{', start)
            depth = 1
            end = brace + 1
            while depth:
                depth += (reset_source[end] == '{') - (reset_source[end] == '}')
                end += 1
            reset_bodies.append(reset_source[start:end])
    (packet / 'extracted/options_button_reset_owner.inc').write_text('\n\n'.join(reset_bodies) + '\n')
    (packet / 'extracted/options-reset-receipt.json').write_text(json.dumps(reset_receipt, indent=2) + '\n')
    # Keep every real OptionsDialog method unchanged. Unrelated pages, ModInfo and
    # taskbar are controlled doubles in this fixture; language page/controls are real.
    source = (repo / 'src/GameUI/OptionsDialog.cpp').read_text(encoding='cp1252')
    body = source[source.index('COptionsDialog::COptionsDialog'):]
    (packet / 'extracted/options_fixture.inc').write_text(body, encoding='utf-8')
    (packet / 'extracted/options-receipt.json').write_text(json.dumps({
        'sourceSha256': hashlib.sha256((repo / 'src/GameUI/OptionsDialog.cpp').read_bytes()).hexdigest(),
        'bodySha256': hashlib.sha256(body.encode()).hexdigest()}, indent=2) + '\n')
if args.driver == 'options_voice':
    for name in ('OptionsSubVoice', 'OptionsSubAudio', 'CvarSlider', 'CvarToggleCheckButton',
                 'LabeledCommandComboBox', 'ModInfo'):
        selected.append(dict(owner, file=str(repo / 'src/GameUI' / (name + '.cpp'))))
    driver_source = repo / 'tests/native/options_voice/options_voice_tests.cpp'
elif args.driver == 'label_layout':
    driver_source = repo / 'tests/native/live_localization_vgui/label_layout_tests.cpp'
elif args.driver == 'live_localization':
    driver_source = repo / 'tests/native/live_localization_vgui/runtime_tests.cpp'
elif args.driver == 'zbot_layout':
    driver_source = repo / 'tests/native/zbot_layout/layout_tests.cpp'
else:
    driver_source = repo / ('tests/native/server_browser_ui/' + args.driver + '_tests.cpp')
selected.append(dict(owner, file=str(driver_source)))

# The live Options fixture includes the production State inline in its driver.
# Record its source/header inputs as one snapshot so concurrent control/header
# edits cannot silently produce a mixed component packet.
def options_inputs():
    paths = {Path(entry['file'].replace(source_prefix + '/src', str(repo / 'src')))
             for entry in selected}
    for directory in ('src/public', 'src/vgui2', 'src/gameui', 'src/common', 'src/engine', 'src/filesystem'):
        paths.update(path for path in (repo / directory).rglob('*')
                     if path.is_file() and path.suffix in ('.h', '.inc'))
    paths.update(repo / relative for relative in (
        'src/engine/localization_state.h', 'src/common/interface_language.h',
        'src/common/interface_language_preference.h', 'src/gameui/OptionsDialog.cpp',
        'src/gameui/OptionsDialog.h', 'src/gameui/OptionsSubInterface.h',
        'src/gameui/OptionsSubVideo.cpp', 'src/gameui/OptionsSubAdvanced.cpp',
        'src/gameui/OptionsSubKeyboard.cpp'))
    return {str(path.relative_to(repo)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in sorted(paths) if path.is_file()}

options_snapshot = options_inputs() if args.driver == 'interface_options' else None
if options_snapshot is not None:
    (packet / 'options-inputs-before.json').write_text(json.dumps(options_snapshot, indent=2) + '\n')

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
if options_snapshot is not None:
    after = options_inputs()
    (packet / 'options-inputs-after.json').write_text(json.dumps(after, indent=2) + '\n')
    dependencies = {'src/gameui/optionsdialog.cpp', 'src/gameui/optionssubvideo.cpp',
                    'src/gameui/optionssubadvanced.cpp', 'src/gameui/optionssubkeyboard.cpp'}
    for result in results:
        dependency_file = Path(result['object']).with_suffix('.d')
        if dependency_file.is_file():
            words = shlex.split(dependency_file.read_text().replace('\\\n', ' '))
            for word in words[1:]:
                try:
                    dependencies.add(str(Path(word).relative_to(repo)).casefold())
                except ValueError:
                    pass
    relevant = {path: digest for path, digest in options_snapshot.items() if path.casefold() in dependencies}
    (packet / 'options-relevant-inputs.json').write_text(json.dumps(relevant, indent=2) + '\n')
    changed = sorted(path for path, digest in relevant.items() if after.get(path) != digest)
    if changed:
        raise RuntimeError('Options source/header inputs changed during component build: ' + ', '.join(changed))
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
