"""Compile actual visibility bodies with owning runtime policies and ASan/UBSan."""
import argparse
import json
from pathlib import Path
import shlex
import subprocess

root = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser()
parser.add_argument('build', type=Path)
parser.add_argument('--source-revision')
parser.add_argument('--case', default='all')
parser.add_argument('--assets', type=Path)
args = parser.parse_args()
output = args.build.resolve() / 'pvs-tests'
output.mkdir(exist_ok=True)

def production(name):
    if args.source_revision:
        return subprocess.check_output(['git', 'show', args.source_revision + ':src/engine/' + name],
                                       cwd=root, text=True)
    return (root / 'src/engine' / name).read_text()

text = production('cmodel.cpp')
text = text[text.index('static byte* gPAS;'):text.index('/*\n=============\nCM_HeadnodeVisible')]
(output / 'visibility.inc').write_text(text)
def function(text, signature):
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

entries = json.loads((args.build / 'compile_commands.json').read_text())
for mode, source in [('hardware', 'gl_model.cpp'), ('software', 'model.cpp')]:
    owner = production(source)
    owner_functions = ['ModelVisibilityInfo Mod_GetVisibilityInfo(', 'void Mod_FillInCRCInfo(',
                       'model_t* Mod_FindName(', 'void Mod_ClearAll()', 'void Mod_ChangeGame()',
                       'void Mod_LoadVisibility(', 'void Mod_LoadLeafs(', 'float RadiusFromBounds(']
    if args.source_revision:
        owner_functions = []
    parts = []
    if owner_functions:
        start = owner.index('typedef struct')
        end = owner.index('} mod_known_info_t;', start) + len('} mod_known_info_t;')
        parts += [owner[start:end], 'model_t mod_known[MAX_KNOWN_MODELS];\nint mod_numknown;\n'
                  'model_t* loadmodel;\nbyte* mod_base;\nchar loadname[32];\n'
                  'static mod_known_info_t mod_known_info[MAX_KNOWN_MODELS];']
        for signature in owner_functions:
            parts.append(function(owner, signature))
        if mode == 'software':
            parts.append(function(owner, 'void SW_Mod_Init()'))
        inline = owner[owner.index('// set up the submodels'):]
        loop = function(inline, 'for (')
        parts.append('static void LoadInlineModels(model_t* mod) {\n' +
                     ('int i; int j; dmodel_t* bm; char name[10];\n' if mode == 'hardware' else '') + loop + '\n}')
    (output / 'owner.inc').write_text('\n\n'.join(parts))
    entry = next(e for e in entries if e['file'].endswith('/' + source))
    command = shlex.split(entry['command'])
    command = command[:command.index('-o')]
    while '-include' in command:
        index = command.index('-include')
        del command[index:index + 2]
    command = [arg for arg in command if arg not in ('-DNDEBUG', '-fno-exceptions')]
    binary = output / mode
    command += ['-O1', '-g', '-fexceptions', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                '-I' + str(output)]
    if args.source_revision:
        command += ['-DPVS_BASELINE']
    command += [str(root / 'tests/native/pvs_decompression/pvs_tests.cpp'), '-o', str(binary)]
    subprocess.run(command, cwd=root, check=True)
    run = [str(binary), args.case]
    if args.assets:
        run += [str(args.assets.resolve())]
    print('RUN', shlex.join(run), flush=True)
    subprocess.run(run, cwd=root, check=True)
