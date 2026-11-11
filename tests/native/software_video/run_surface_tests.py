"""Compile actual-image software surface regressions with the owning source policy."""
from pathlib import Path
import json
import os
import shlex
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
build = Path(sys.argv[1]).resolve()
entry = next(e for e in json.loads((build / 'compile_commands.json').read_text())
             if e['file'].endswith('/vid_win.cpp'))
args = shlex.split(entry['command'])
args = args[:args.index('-o')]
while '-include' in args:
    offset = args.index('-include')
    del args[offset:offset + 2]
args = [arg for arg in args if arg not in ('-DNDEBUG', '-fno-exceptions')]
runtime = next((build / 'runtime').glob('*/swds.dylib')).parent
output = build / 'surface-tests'
output.mkdir(exist_ok=True)
env = dict(os.environ, SDL_VIDEO_DRIVER='dummy', SDL_RENDER_DRIVER='software', DYLD_LIBRARY_PATH=str(runtime))
for name in sys.argv[2:] or ['software_surface_tests', 'software_vgui2_tests']:
    binary = output / name
    command = args + ['-fexceptions', '-Wl,-rpath,' + str(runtime),
                      str(root / 'tests/native/software_video' / (name + '.cpp'))]
    if name == 'software_vgui2_tests':
        command += [str(runtime / 'swds.dylib')]
    command += ['-o', str(binary)]
    print(shlex.join(command), flush=True)
    subprocess.run(command, cwd=root, check=True)
    command = [str(binary)]
    if name == 'software_surface_tests':
        command += [str(runtime / 'swds.dylib'), str(runtime / 'hw.dylib')]
    subprocess.run(command, cwd=runtime, env=env, check=True)
