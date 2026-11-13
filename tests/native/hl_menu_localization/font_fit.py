#!/usr/bin/env python3
"""Measure actual prefixed menu labels with the actual VGUI1 Font/native-face path."""
import argparse
import importlib.util
from pathlib import Path
import shlex
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--root', type=Path, default=Path('.'))
p.add_argument('--evidence', type=Path, required=True)
p.add_argument('--provider-build', type=Path, required=True)
p.add_argument('--font', type=Path, default=Path('/System/Library/Fonts/Supplemental/Arial.ttf'))
a = p.parse_args()
root = a.root.resolve()
out = a.evidence.resolve()
owner = a.provider_build.resolve()
spec = importlib.util.spec_from_file_location('extract', root / 'tests/native/network_platform/extract_functions.py')
extract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(extract)
source = (root / 'src/utils/vgui/lib-src/vgui/Font.cpp').read_text()
(out / 'font.inc').write_text(extract.extract(source, 'Font::getTextSize'))
commands = subprocess.check_output(['ninja', '-C', str(owner), '-t', 'commands', 'vgui1_utf8_text'], text=True).splitlines()
compile_command = shlex.split(commands[0])
compile_command.insert(1, '-I' + str(out))
object_file = str(out / 'font-fit.o')
for flag, value in (('-o', object_file), ('-MF', str(out / 'font-fit.d')), ('-MT', object_file)):
    compile_command[compile_command.index(flag) + 1] = value
compile_command[-1] = str(root / 'tests/native/hl_menu_localization/font_fit.cpp')
subprocess.run(compile_command, cwd=owner, check=True)
link = shlex.split(commands[-1])
link = link[2:-2]
link[link.index('-o') - 1] = object_file
link[link.index('-o') + 1] = str(out / 'font_fit')
subprocess.run(link, cwd=owner, check=True)
failed = False
for language in ('russian', 'polish'):
    result = subprocess.run([str(out / 'font_fit'), str(a.font), str(out / f'{language}.titles')],
                            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (out / f'{language}-font-fit.tsv').write_text(result.stdout)
    print(language, result.stdout)
    failed = failed or result.returncode != 0
raise SystemExit(1 if failed else 0)
