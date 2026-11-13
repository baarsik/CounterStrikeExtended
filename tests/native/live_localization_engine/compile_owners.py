#!/usr/bin/env python3
"""Syntax-check changed owners with their actual generated production policies."""
import concurrent.futures
import json
from pathlib import Path
import shlex
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
manifest = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)
commands = json.loads(manifest.read_text())
names = ['engine/filesystem.cpp', 'engine/localization.cpp', 'engine/tmessage.cpp',
         'engine/tmessage_localization.cpp', 'engine/vgui2/BaseUISurface.cpp',
         'engine/vgui2/BaseUI_Interface.cpp', 'engine/vgui2/FontTextureCache.cpp',
         'engine/host.cpp', 'engine/gl_screen.cpp', 'common/interface_language_preference.cpp',
         'vgui2/vgui_surfacelib/FontManager.cpp', 'filesystem/BaseFileSystem.cpp', 'filesystem/localized_filesystem.cpp']

def check(name):
    suffix = name
    if name in ('engine/localization.cpp', 'engine/tmessage_localization.cpp'):
        suffix = 'engine/filesystem.cpp'
    if name == 'filesystem/localized_filesystem.cpp':
        suffix = 'filesystem/BaseFileSystem.cpp'
    original = next(item for item in commands if item['file'].endswith('/' + suffix))
    args = shlex.split(original['command'])
    filtered = []
    index = 0
    while index < len(args):
        if args[index] == '-o':
            index += 2
        elif args[index] == '-c':
            index += 1
        elif args[index] == original['file']:
            index += 1
        else:
            filtered.append(args[index])
            index += 1
    filtered.extend(['-fsyntax-only', str(root / 'src' / name)])
    result = subprocess.run(filtered, cwd=original['directory'], text=True, capture_output=True)
    (out / (name.replace('/', '_') + '.log')).write_text(result.stdout + result.stderr)
    return {'file': name, 'exit': result.returncode, 'command': filtered,
            'errors': '\n'.join(line for line in result.stderr.splitlines() if 'error:' in line)}

with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:
    results = list(executor.map(check, names))
(out / 'result.json').write_text(json.dumps(results, indent=2) + '\n')
for result in results:
    print(result['file'], result['exit'], result['errors'])
raise SystemExit(any(result['exit'] for result in results))
