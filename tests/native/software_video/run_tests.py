#!/usr/bin/env python3
"""Use the owning swds compile policy and its built SDL, not a replacement renderer."""
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
root = Path(__file__).resolve().parents[3]
build = Path(sys.argv[1]).resolve()
entries = json.loads((build / 'compile_commands.json').read_text())
entry = next(e for e in entries if e['file'].endswith('/vid_win.cpp'))
args = shlex.split(entry['command'])
args = args[:args.index('-o')]
# The component tests replace external engine services and do not load Steam.
for flag in ('-include',):
    while flag in args:
        index = args.index(flag)
        del args[index:index+2]
args = [a for a in args if a not in ('-DNDEBUG', '-fno-exceptions')]
args += ['-fexceptions', '-Wl,-dead_strip']
sdl = next((build / 'dependencies/SDL3/lib').rglob('libSDL3.0.dylib'))
webp = build / 'lib' / sdl.parent.name / 'webp/md/libcse_webp_md.a'
args += ['-Wl,-rpath,' + str(sdl.parent)]
out = build.parent / 'tests'
out.mkdir(exist_ok=True)
def run(command, env=None):
    print(shlex.join(map(str, command)), flush=True)
    subprocess.run(list(map(str, command)), check=True, cwd=root, env=env)
run(args + [root / 'tests/native/software_video/video_buffers_tests.cpp', root / 'src/common/image_codec.cpp',
            sdl, webp, '-o', out / 'video_buffers_tests'])
for name in ('software_checked_allocations', 'software_snapshot_rows', 'software_pixels_and_palette'):
    run([out / 'video_buffers_tests', name])
    print('PASS', name, flush=True)
run(args + [root / 'tests/native/software_video/presenter_tests.cpp',
            root / 'src/engine/native_software_video.cpp', sdl, '-o', out / 'presenter_tests'])
run([out / 'presenter_tests'])
print('PASS software_video_lifecycle (real SDL dummy upload/readback)', flush=True)

run(args + [root / 'tests/native/software_video/presenter_failure_tests.cpp', sdl,
            '-o', out / 'presenter_failure_tests'])
run([out / 'presenter_failure_tests'])
print('PASS software_video_lifecycle (controlled SDL allocation/texture configuration failures)', flush=True)

run(args + [root / 'tests/native/software_video/retained_pixels_tests.cpp',
            root / 'src/engine/native_software_video.cpp', sdl, '-o', out / 'retained_pixels_tests'])
os.environ['DYLD_LIBRARY_PATH'] = str(next((build / 'runtime').glob('*/swds.dylib')).parent)
run([out / 'retained_pixels_tests', next((build / 'runtime').glob('*/swds.dylib'))])

run([sys.executable, root / 'tests/native/software_video/run_surface_tests.py', build])

run(args + [root / 'tests/native/software_video/video_mode_tests.cpp',
            root / 'src/engine/native_software_video.cpp', sdl, '-o', out / 'video_mode_tests'])
run([out / 'video_mode_tests'])
print('PASS software_video_lifecycle (actual IVideoMode partial window/renderer failure, aliases, restore, shutdown)', flush=True)

run(args + [root / 'tests/native/software_video/studio_heap_tests.cpp', '-o', out / 'studio_heap_tests'])
run([out / 'studio_heap_tests', next((build / 'runtime').glob('*/swds.dylib'))])
print('PASS software_checked_allocations (actual studio cache/heap generation)', flush=True)
run(args + [root / 'tests/native/software_video/dedicated_load_without_video.cpp',
            root / 'src/launcher/native/SocketImageOwner.cpp', sdl, '-Wl,-export_dynamic',
            '-o', out / 'dedicated_load_without_video'])
run(args + ['-dynamiclib', root / 'tests/native/software_video/forbid_video.cpp', sdl,
            '-o', out / 'forbid_video.dylib'])
# Compile the exact production function and declarations, substituting only its
# subsystem boundaries. No copy of the shutdown algorithm lives in the fixture.
source = (root / 'src/engine/host.cpp').read_text()
owned = source[:source.index('\nqboolean Host_FilterTime')]
fixture = out / 'host_shutdown_actual.cpp'
fixture.write_text(owned + '\n' + (root / 'tests/native/software_video/host_shutdown_fixture.cpp').read_text())
run(args + ['-Wno-unused-variable', fixture, '-o', out / 'host_shutdown_test'])
run([out / 'host_shutdown_test'])
print('PASS native Host_Shutdown ordering, reentry, and staged GameUI/VGUI completion', flush=True)
video_forbidden = dict(os.environ, DYLD_INSERT_LIBRARIES=str(out / 'forbid_video.dylib'))
run([out / 'dedicated_load_without_video', next((build / 'runtime').glob('*/swds.dylib'))], env=video_forbidden)

run([sys.executable, root / "tests/native/software_video/run_dedicated_owner_test.py", out])
