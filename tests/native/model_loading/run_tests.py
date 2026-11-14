"""Run the actual studio/sprite loaders with controlled allocator/upload boundaries."""
from pathlib import Path
import argparse
import json
import shlex
import subprocess

root = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser()
parser.add_argument('build', type=Path)
parser.add_argument('--baseline', action='store_true')
parser.add_argument('--heap-only', action='store_true')
parser.add_argument('--boundaries-only', action='store_true')
parser.add_argument('--source-revision', help='Read production loader bodies from a Git revision (for baseline RED).')
parser.add_argument('--case', default='all')
parser.add_argument('--assets', type=Path)
parser.add_argument('--mode', choices=['hardware', 'software', 'both'], default='both')
options = parser.parse_args()
build = options.build.resolve()
output = build / 'model-loading-tests'
output.mkdir(exist_ok=True)
def production(name):
    if options.source_revision:
        return subprocess.check_output(['git', 'show', options.source_revision + ':src/engine/' + name],
                                       cwd=root, text=True)
    return (root / 'src/engine' / name).read_text()

(output / 'studio_loader.inc').write_text(production('l_studio.cpp'))
zone = production('zone.cpp')
hunk_start = zone.index('void* Hunk_AllocName(')
cache_start = zone.index('void* Cache_Alloc(')
hunk_expression = next(line.strip() for line in zone[hunk_start:].splitlines() if 'size = sizeof( hunk_t )' in line)
temp_start = zone.index('void* Hunk_TempAlloc(')
temp_expression = next(line.strip() for line in zone[temp_start:].splitlines() if 'size = ( size + 15 )' in line)
cache_expression = next(line.strip() for line in zone[cache_start:].splitlines() if 'size = ( size + sizeof( cache_system_t )' in line)
(output / 'allocator_expressions.inc').write_text(
    'static int ActualHunkTotal(int size) {\n    ' + hunk_expression + '\n    return size;\n}\n' +
    'static int ActualCacheTotal(int size) {\n    ' + cache_expression + '\n    return size;\n}\n' +
    'static int ActualTemporaryHunkPayload(int size) {\n    ' + temp_expression + '\n    return size;\n}\n')
entries = json.loads((build / 'compile_commands.json').read_text())
for mode, source in [('hardware', 'gl_model.cpp'), ('software', 'model.cpp')]:
    if options.heap_only or options.mode not in ('both', mode):
        continue
    entry = next(e for e in entries if e['file'].endswith('/' + source))
    args = shlex.split(entry['command'])
    args = args[:args.index('-o')]
    while '-include' in args:
        index = args.index('-include')
        del args[index:index + 2]
    args = [arg for arg in args if arg not in ('-DNDEBUG', '-fno-exceptions')]
    if not options.baseline:
        boundary = output / (mode + '-allocation-boundaries')
        subprocess.run(args + ['-O1', '-g', '-fexceptions', '-fsanitize=address,undefined',
                       '-fno-sanitize-recover=all', '-I' + str(output),
                       str(root / 'tests/native/model_loading/allocation_boundary_tests.cpp'),
                       '-o', str(boundary)], cwd=root, check=True)
        subprocess.run([str(boundary)], cwd=root, check=True)
    if options.boundaries_only:
        continue
    text = production(source)
    start = text.index('static void* LoadSpriteFrame(') if mode == 'hardware' else text.index('void* Mod_LoadSpriteFrame(')
    end = text.index('/*\n=================\nMod_FloodFillSkin', start) if mode == 'hardware' else text.index('/*\n=================\nMod_LoadAliasFrame', start)
    (output / 'sprite_loader.inc').write_text(text[start:end])
    binary = output / mode
    command = args + ['-O1', '-g', '-fexceptions', '-fsanitize=address,undefined',
                      '-fno-sanitize-recover=all', '-I' + str(output)]
    if options.baseline:
        command += ['-DMODEL_LOADING_BASELINE']
    command += [str(root / 'tests/native/model_loading/model_loading_tests.cpp'), '-o', str(binary)]
    subprocess.run(command, cwd=root, check=True)
    command = [str(binary), options.case]
    if options.assets:
        command += [str(options.assets.resolve())]
    print('RUN', shlex.join(command), flush=True)
    subprocess.run(command, cwd=root, check=True)

if options.boundaries_only:
    raise SystemExit(0)

common = production('common.cpp')
start = common.index('enum\n{\n\tLOADFILE_ZONE')
end = common.index('byte* COM_LoadHunkFile(', start)
# Select the real legacy/native preprocessor branch while retaining the host ABI and sanitizers.
(output / 'heap_reader.inc').write_text(common[start:end].replace('INTPTR_MAX > INT32_MAX', 'MODEL_TEST_NATIVE'))
for native in (0, 1):
    binary = output / ('heap-reader-' + str(native))
    command = ['/usr/bin/c++', '-std=c++17', '-O1', '-g', '-fsanitize=address,undefined',
               '-fno-sanitize-recover=all', '-DMODEL_TEST_NATIVE=' + str(native), '-I' + str(output),
               str(root / 'tests/native/model_loading/heap_reader_tests.cpp'), '-o', str(binary)]
    subprocess.run(command, cwd=root, check=True)
    print('RUN', binary, flush=True)
    subprocess.run([str(binary)], cwd=root, check=True)

if not options.baseline and not options.heap_only:
    entry = next(e for e in entries if e['file'].endswith('/model.cpp'))
    consumer_args = shlex.split(entry['command'])
    consumer_args = consumer_args[:consumer_args.index('-o')]
    while '-include' in consumer_args:
        index = consumer_args.index('-include')
        del consumer_args[index:index + 2]
    consumer_args = [arg for arg in consumer_args if arg not in ('-DNDEBUG', '-fno-exceptions')]
    for name in ('studio_cache_tests.cpp', 'studio_heap_tests.cpp'):
        subprocess.run(consumer_args + ['-fexceptions', '-fsyntax-only',
                       str(root / 'tests/native/software_video' / name)], cwd=root, check=True)
        print('PASS direct loader consumer compilation:', name, flush=True)
