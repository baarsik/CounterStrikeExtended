"""Run the real hardware-engine texture kernels with GL capture and ASan/UBSan."""
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess

root = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser()
parser.add_argument('build', type=Path)
parser.add_argument('--source-revision')
parser.add_argument('--case', default='all')
options = parser.parse_args()
output = options.build.resolve() / 'texture-upload-tests'
output.mkdir(exist_ok=True)
source_path = 'src/engine/gl_draw.cpp'
source = (subprocess.check_output(['git', 'show', options.source_revision + ':' + source_path], cwd=root).decode()
          if options.source_revision else (root / source_path).read_text())


def function(name):
    match = re.search(r'(?m)^(?:static )?(?:void|int|float|bool|unsigned int) ' + name + r'\([^)]*\)\s*\{', source)
    if not match:
        raise ValueError('Missing production function: ' + name)
    masked = re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                    lambda item: ' ' * len(item.group()), source)
    end = masked.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (masked[end] == '{') - (masked[end] == '}')
        end += 1
    return source[match.start():end]


names = ['ComputeScaledSize', 'GL_ResampleTexturePoint', 'PaletteRGB', 'GL_Upload16', 'GL_MipMap',
         'GL_Upload32', 'BoxFilter3x3', 'GL_ResampleTexture', 'GL_ResampleAlphaTexture']
if 'static bool GL_TextureDimensions(' in source:
    names.insert(1, 'GL_TextureDimensions')
if 'float GL_TextureAnisotropy(' in source:
    names.insert(0, 'GL_TextureAnisotropy')
unpack = re.search(r'class TextureUnpackState \{[\s\S]*?\n\};', source)
prefix = unpack.group() + '\n\n' if unpack else ''
(output / 'texture_functions.inc').write_text(prefix + '\n\n'.join(function(name) for name in names))
(output / 'texture_cache.inc').write_text(function('GL_LoadTexture2'))
(output / 'texture_bind.inc').write_text(function('GL_SelectTexture') + '\n' + function('GL_Bind'))
entries = json.loads((options.build / 'compile_commands.json').read_text())
entry = next(e for e in entries if e['file'].endswith('/gl_draw.cpp'))
command = shlex.split(entry['command'])
command = command[:command.index('-o')]
while '-include' in command:
    index = command.index('-include')
    del command[index:index + 2]
command = [arg for arg in command if arg not in ('-DNDEBUG', '-fno-exceptions')]
binary = output / 'texture_upload_tests'
command += ['-O1', '-g', '-fexceptions', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
            '-I' + str(output), str(root / 'tests/native/texture_upload/texture_upload_tests.cpp'), '-o', str(binary)]
if options.source_revision:
    command += ['-DTEXTURE_UPLOAD_BASELINE']
subprocess.run(command, cwd=root, check=True)
subprocess.run([str(binary), options.case], cwd=root, check=True)
