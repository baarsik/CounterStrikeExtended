"""Actual browser/channel owners; only the external wait call is failed once, then invokes real SDL."""
import hashlib
import json
import pathlib
import shlex
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[3]
out = pathlib.Path(sys.argv[1]).resolve()
out.mkdir(parents=True, exist_ok=True)
sdl = pathlib.Path(sys.argv[2]).resolve()
commands = []
common = ['clang++', '-std=c++17', ('-Os' if '--release' in sys.argv else '-O0'), '-UNDEBUG', '-Wall', '-Wextra', '-Werror',
          '-I' + str(root / 'src/engine/html/native'), '-I' + str(root / 'src/public'),
          '-I' + str(root / 'src/external/SDL3/include')]
objects = []
for source in [root / 'src/engine/html/native/browser_controller.cpp',
               root / 'src/engine/html/native/process_channel.cpp',
               root / 'tests/native/software_lifetime/controller_completion.cpp']:
    obj = out / (source.stem + '.o')
    command = common + (['-DSDL_WaitProcess=CseControlledWait'] if source.stem == 'process_channel' else [])
    command += ['-c', str(source), '-o', str(obj)]
    commands.append(command)
    subprocess.run(command, check=True)
    objects.append(str(obj))
command = ['clang++', *objects, str(sdl), '-Wl,-rpath,' + str(sdl.parent), '-o', str(out / 'controller_completion')]
commands.append(command)
subprocess.run(command, check=True)
command = [str(out / 'controller_completion'), str(out / 'profile')]
commands.append(command)
subprocess.run(command, check=True)
(out / 'receipt.json').write_text(json.dumps({'commands': commands, 'sdl': str(sdl),
    'sdl_sha256': hashlib.sha256(sdl.read_bytes()).hexdigest()}, indent=2) + '\n')
