#!/usr/bin/env python3
"""Measure retained default bitmap menu font with actual Font/platform/services owners."""
import argparse
from pathlib import Path
import shlex
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--root', type=Path, default=Path('.'))
p.add_argument('--evidence', type=Path, required=True)
p.add_argument('--provider-build', type=Path, required=True)
p.add_argument('--bitmap', type=Path, required=True)
p.add_argument('--font', type=Path, default=Path('/System/Library/Fonts/Supplemental/Arial.ttf'))
a = p.parse_args()
root = a.root.resolve()
out = a.evidence.resolve()
owner = a.provider_build.resolve()
commands = subprocess.check_output(['ninja', '-C', str(owner), '-t', 'commands', 'vgui1_utf8_bitmap_owner'],
                                   text=True).splitlines()
compile_command = shlex.split(commands[0])
object_file = str(out / 'bitmap-fit.o')
for flag, value in (('-o', object_file), ('-MF', str(out / 'bitmap-fit.d')), ('-MT', object_file)):
    compile_command[compile_command.index(flag) + 1] = value
compile_command[-1] = str(root / 'tests/native/hl_menu_localization/bitmap_fit.cpp')
subprocess.run(compile_command, cwd=owner, check=True)
link = shlex.split(commands[-1])[2:-2]
main_object = next(index for index, value in enumerate(link) if value.endswith('/bitmap_owner.cpp.o'))
link[main_object] = object_file
link[link.index('-o') + 1] = str(out / 'bitmap_fit')
subprocess.run(link, cwd=owner, check=True)
failed = False
for language in ('russian', 'polish'):
    result = subprocess.run([str(out / 'bitmap_fit'), str(a.font), str(a.bitmap), str(out / f'{language}.titles')],
                            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (out / f'{language}-bitmap-fit.tsv').write_text(result.stdout)
    print(language, result.stdout)
    failed = failed or result.returncode != 0
raise SystemExit(1 if failed else 0)
