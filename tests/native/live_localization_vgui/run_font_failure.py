#!/usr/bin/env python3
"""Compile actual font owners and exercise non-destructive failed replacement."""
import json
from pathlib import Path
import shlex
import subprocess

root = Path(__file__).resolve().parents[3]
out = root / 'build/live-localization/font-failure'
out.mkdir(exist_ok=True)
manifest = json.loads((root / 'build/runtime-macos-arm64-release/compile_commands.json').read_text())
entry = next(item for item in manifest if '/vgui2.dir/' in item['output'] and item['file'].endswith('/FontManager.cpp'))
command = shlex.split(entry['command'])
index = command.index('-o')
del command[index:index + 2]
command.remove('-c')
command.remove(entry['file'])
command = [value for value in command if value != '-Werror']
font = root / 'src/vgui2/vgui_surfacelib'
command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-I' + str(font),
            str(root / 'tests/native/live_localization_vgui/font_failure_tests.cpp')]
command += [str(font / name) for name in ('FontManager.cpp', 'FontAmalgam.cpp', 'linuxfont.cpp', 'native/MacFontDiscovery.cpp')]
# Reuse the admitted actual provider's pinned FreeType archive.
native_include = next(Path(command[index + 1]) for index, value in enumerate(command[:-1])
    if value == '-isystem' and (Path(command[index + 1]).parent / 'lib/libfreetype.a').is_file())
library = native_include.parent / 'lib/libfreetype.a' 
command += [str(library), str(library.parent / 'libpng.a'), str(library.parent / 'libbrotlidec.a'),
            str(library.parent / 'libbrotlicommon.a'), '-framework', 'CoreText', '-framework', 'CoreFoundation', '-lz', '-lbz2',
            '-o', str(out / 'font-failure-tests')]
result = subprocess.run(command, cwd=entry['directory'], text=True, capture_output=True)
(out / 'build.log').write_text(result.stdout + result.stderr)
(out / 'command.json').write_text(json.dumps(command, indent=2) + '\n')
if result.returncode:
    print(result.stderr)
    raise SystemExit(result.returncode)
result = subprocess.run([str(out / 'font-failure-tests')], text=True, capture_output=True)
(out / 'run.log').write_text(result.stdout + result.stderr)
print(result.stdout + result.stderr)
raise SystemExit(result.returncode)
