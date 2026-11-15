"""Exercise the private BSP preflight with both production model layouts and sanitizers."""
import argparse
import json
import re
from pathlib import Path
import shlex
import subprocess

root = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser()
parser.add_argument('build', type=Path)
parser.add_argument('--baseline', action='store_true')
parser.add_argument('--assets', type=Path)
options = parser.parse_args()
output = options.build.resolve() / 'bsp-loading-tests'
output.mkdir(exist_ok=True)
entries = json.loads((options.build / 'compile_commands.json').read_text())


def function(text, name):
    match = re.search(r'(?m)^(?:static )?(?:void|int|byte\*|colorVec) ' + name + r'\([^)]*\)\s*\{', text)
    masked = re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                    lambda item: ' ' * len(item.group()), text)
    end = masked.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (masked[end] == '{') - (masked[end] == '}')
        end += 1
    return text[match.start():end]


for mode, source in [('hardware', 'gl_model.cpp'), ('dedicated', 'model.cpp')]:
    owner = (root / 'src/engine' / source).read_text()
    names = ['Mod_LoadEntities', 'Mod_LoadVertexes', 'Mod_LoadEdges', 'Mod_LoadSurfedges', 'Mod_LoadLighting',
             'Mod_LoadVisibility', 'Mod_LoadPlanes', 'Mod_LoadTexinfo', 'CalcSurfaceExtents',
             'Mod_LoadFaces', 'Mod_LoadMarksurfaces', 'Mod_LoadLeafs', 'Mod_SetParent',
             'Mod_LoadNodes', 'Mod_LoadClipnodes', 'Mod_LoadSubmodels', 'Mod_MakeHull0',
             'Mod_LoadTextures', 'Mod_LoadBrushModel']
    if mode == 'hardware':
        names.insert(0, 'Mod_LumpCount')
    (output / 'brush_loader.inc').write_text('\n\n'.join(function(owner, name) for name in names))
    light_source = 'gl_rlight.cpp' if mode == 'hardware' else 'r_light.cpp'
    light_owner = (root / 'src/engine' / light_source).read_text()
    (output / 'light_trace.inc').write_text(function(light_owner, 'RecursiveLightPoint'))
    server = (root / 'src/engine/sv_main.cpp').read_text()
    fat_functions = ['SV_FatPVS', 'SV_FatPAS']
    (output / 'fat_visibility.inc').write_text('\n\n'.join(function(server, name) for name in fat_functions))
    entry = next(e for e in entries if e['file'].endswith('/' + source))
    command = shlex.split(entry['command'])
    command = command[:command.index('-o')]
    while '-include' in command:
        index = command.index('-include')
        del command[index:index + 2]
    command = [arg for arg in command if arg not in ('-DNDEBUG', '-fno-exceptions')]
    binary = output / mode
    command += ['-O1', '-g', '-fexceptions', '-fsanitize=address,undefined', '-fno-sanitize-recover=all']
    if options.baseline:
        command += ['-DBSP_LOADING_BASELINE']
    command += ['-I' + str(output), str(root / 'tests/native/bsp_loading/bsp_tests.cpp'), '-o', str(binary)]
    subprocess.run(command, cwd=root, check=True)
    run = [str(binary)]
    if options.assets:
        run += [str(options.assets.resolve())]
    print('RUN', shlex.join(run), flush=True)
    subprocess.run(run, cwd=root, check=True)
