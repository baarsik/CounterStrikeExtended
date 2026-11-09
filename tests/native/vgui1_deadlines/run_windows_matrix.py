"""Real-method Windows controls and original-provider selected-token comparison."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

p = argparse.ArgumentParser()
p.add_argument('--source', type=Path, required=True)
p.add_argument('--baseline', type=Path, required=True)
p.add_argument('--art', type=Path, required=True)
p.add_argument('--work', type=Path, required=True)
a = p.parse_args()
art = a.art.resolve()
work = a.work.resolve()
work.mkdir(parents=True, exist_ok=True)
art.mkdir(parents=True, exist_ok=True)
root = work / 'source'
shutil.copytree(a.source, root, dirs_exist_ok=True)
baseline = work / 'baseline'
shutil.copytree(a.baseline, baseline, dirs_exist_ok=True)
tests = root / 'tests/native/vgui1_deadlines'
vgui = root / 'GoldSrc-SDK/utils/vgui/lib-src/vgui'
fixture = work / 'controlled-bindings'
subprocess.run([sys.executable, str(tests / 'prepare_fixture.py'), '--root', str(root), '--output', str(fixture)], check=True)
rows = []
token_rows = []


def tokens(data):
    # Preserve complete escaped string/character literals; ignore only whitespace.
    result = re.findall(rb'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[A-Za-z_][A-Za-z_0-9]*|[0-9]+|[^\s]', data)
    assert len(result) > 1000, 'unknown/empty preprocessor report'
    return result


def run(name, phase, command, env, cwd):
    result = subprocess.run([str(x) for x in command], cwd=cwd, env=env, capture_output=True, timeout=120)
    (art / (name + '-' + phase + '.log')).write_bytes(result.stdout + result.stderr)
    rows.append({'name': name, 'phase': phase, 'command': [str(x) for x in command], 'exit': result.returncode})
    (art / 'windows-commands.json').write_text(json.dumps(rows, indent=2) + '\n')
    assert result.returncode == 0, rows[-1]
    return result.stdout


for cpu in ['x86', 'x64', 'arm64']:
    setup = work / ('setup-' + cpu + '.cmd')
    setup.write_text('@echo off\ncall "C:\\BuildTools\\2026\\Common7\\Tools\\VsDevCmd.bat" -no_logo -host_arch=arm64 -arch=' + cpu + ' >nul\nif errorlevel 1 exit /b 1\nset\n')
    result = subprocess.run(['cmd.exe', '/d', '/c', str(setup)], capture_output=True, text=True, cwd=work, check=True)
    env = dict(os.environ)
    for line in result.stdout.splitlines():
        if '=' in line and not line.startswith('='):
            key, value = line.split('=', 1)
            env[key] = value
    compiler = Path(shutil.which('cl.exe', path=env.get('Path', env.get('PATH'))))
    (art / ('compiler-' + cpu + '.json')).write_text(json.dumps({'path': str(compiler), 'sha256': hashlib.sha256(compiler.read_bytes()).hexdigest(),
        'INCLUDE': env.get('INCLUDE', ''), 'CL': env.get('CL', ''), '_CL_': env.get('_CL_', '')}, indent=2) + '\n')
    for configuration in ['Release', 'Debug']:
        common = ['/nologo', '/std:c++14', '/EHsc', '/fp:precise', '/W4', '/WX', '/DVGUI_EXPORTS',
                  '/DWIN32', '/D_WINDOWS', '/D_USRDLL', '/Dvgui_EXPORTS']
        common += ['/O2', '/MT', '/DNDEBUG'] if configuration == 'Release' else ['/Od', '/Zi', '/MTd', '/D_DEBUG']
        # Original private provider declaring context and visibility, before/after.
        pp = common + ['/I' + str(vgui / 'include'), '/I' + str(vgui), '/I' + str(vgui / 'common'), '/EP']
        name = cpu + '-' + configuration
        for file in ['App.cpp', 'TextEntry.cpp', 'EditPanel.cpp']:
            old = baseline / 'GoldSrc-SDK/utils/vgui/lib-src/vgui' / file
            before = run(name, file + '-before-preprocess', [compiler] + pp + [old], env, work)
            after = run(name, file + '-after-preprocess', [compiler] + pp + [vgui / file], env, work)
            old_tokens = tokens(before)
            new_tokens = tokens(after)
            assert old_tokens == new_tokens, (name, file, 'Windows selected tokens changed')
            token_rows.append({'context': name, 'source': file, 'tokens': len(old_tokens), 'equal': True,
                'sha256': hashlib.sha256(b'\0'.join(old_tokens)).hexdigest()})
        # Same-context mutation proves that the checker rejects a real selected edit.
        if cpu == 'x86' and configuration == 'Release':
            negative = work / 'negative' / 'EditPanel.cpp'
            negative.parent.mkdir(exist_ok=True)
            text = (vgui / 'EditPanel.cpp').read_text()
            original = '_cursorNextBlinkTime = getApp()->getTimeMillis() + 400;'
            assert text.count(original) == 1
            negative.write_text(text.replace(original, original.replace('400', '401')))
            bad = run(name, 'CONTROL-selected-401-preprocess', [compiler] + pp + [negative], env, work)
            good = run(name, 'CONTROL-selected-original-preprocess', [compiler] + pp + [vgui / 'EditPanel.cpp'], env, work)
            assert tokens(bad) != tokens(good), 'selected-code negative control was accepted'
        (art / 'windows-selected-tokens.json').write_text(json.dumps(token_rows, indent=2) + '\n')
        for view in ['private', 'public']:
            name = cpu + '-' + view + '-' + configuration
            build = work / name
            build.mkdir(exist_ok=True)
            opts = common + ['/I' + str(fixture / ('headers-' + view)), '/I' + str(vgui / 'include'), '/I' + str(vgui),
                '/I' + str(vgui / 'posix'), '/I' + str(tests), '/FI' + str(tests / 'visibility_neutral_fixture.h')]
            files = [tests / 'deadline_reference.cpp', tests / 'controlled_dependencies.cpp', fixture / 'TextEntry.cpp',
                fixture / 'EditPanel.cpp', fixture / 'Panel-controlled-unreachable-property.cpp', vgui / 'App.cpp',
                vgui / 'Color.cpp', fixture / 'App-controlled-clock.cpp', vgui / 'posix/VGui1AppPlatform.cpp']
            objects = []
            for index, file in enumerate(files):
                output = build / ('part' + str(index) + '.obj')
                run(name, 'compile-' + str(index), [compiler] + opts + ['/c', file, '/Fo' + str(output)], env, build)
                objects.append(output)
            exe = build / 'deadline_reference.exe'
            run(name, 'link', [compiler] + opts + objects + ['/Fe' + str(exe)], env, build)
            output = build / 'actual-methods.jsonl'
            run(name, 'check-747-rows', [sys.executable, tests / 'run_and_check.py', exe, output], env, build)
            shutil.copy2(output, art / (name + '.jsonl'))
            shutil.copy2(exe, art / (name + '.exe'))
(art / 'windows-input-hashes.json').write_text(json.dumps({str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
    for p in sorted(root.rglob('*')) if p.is_file()}, indent=2) + '\n')
print('PASS12 Windows method/helper profiles +18 original-provider selected-token equality checks and rejecting401 control')
