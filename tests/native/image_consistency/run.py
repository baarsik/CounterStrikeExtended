#!/usr/bin/env python3
"""Exercise production connection policy, image selection and both engine texture caches."""
import argparse
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--evidence', type=Path, required=True)
parser.add_argument('--webp-library', type=Path, default=Path('build/webp-codec-debug/lib/Debug/webp/md/libcse_webp_md.a'))
parser.add_argument('--webp-include', type=Path, default=Path('build/webp-codec-debug/dependencies/WebP/source/libwebp-1.6.0/src'))
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
out = args.evidence.resolve()
out.mkdir(parents=True, exist_ok=True)


def function(source, signature):
    start = source.index(signature)
    position = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[position] == '{') - (source[position] == '}')
        position += 1
    return source[start:position] + '\n'


zone = (root / 'src/engine/zone.h').read_text()
quakedef = (root / 'src/engine/quakedef.h').read_text()
cache_start = quakedef.index('typedef struct cache_user_s')
cache_end = quakedef.index('} cache_user_t;', cache_start) + len('} cache_user_t;')
zone_start = zone.index('#define ZONE_ID')
zone_end = zone.index('Memory_Init(')
zone_end = zone.rfind('void', zone_start, zone_end)
(out / 'zone_limits.inc').write_text(quakedef[cache_start:cache_end] + '\n' + zone[zone_start:zone_end])
hardware = (root / 'src/engine/gl_warp.cpp').read_text()
start = hardware.index('typedef struct _TargaHeader')
end = hardware.index('static const char* suf[6]', start)
(out / 'hardware_loaders.inc').write_text(hardware[start:end])
software = (root / 'src/engine/r_sky.cpp').read_text()
(out / 'software_loaders.inc').write_text(''.join(function(software, name) for name in (
    'short PutDitheredRGB(', 'static byte* R_CopyWebPImage(', 'static byte* R_LoadTGAImage(',
    'byte* R_LoadTGA(', 'qboolean LoadTGA(')))
main = (root / 'src/engine/cl_main.cpp').read_text()
parse = (root / 'src/engine/cl_parse.cpp').read_text()
(out / 'connection.inc').write_text(''.join(function(main, name) for name in (
    'void CL_RemoveFromResourceList(', 'void CL_ClearClientState(', 'void CL_RegisterResources('))
    + ''.join(function(parse, name) for name in (
        'void CL_ClearResourceList(', 'void CL_ClearResourceLists(', 'void CL_ParseConsistencyInfo(')))
policy = root / 'src/engine/image_consistency.cpp'
(out / 'policy.inc').write_text(policy.read_text()[policy.read_text().index('namespace cse {'):] if policy.exists() else '')
failures = 0
for renderer, suffix in (('hardware', 'HW'), ('software', 'SW')):
    surface = (root / f'src/engine/vgui_EngineSurface{suffix}.cpp').read_text()
    functions = ''
    if renderer == 'hardware':
        functions += function(surface, 'static int SurfaceTexturePowerOfTwo(')
        functions += function(surface, 'static std::uint32_t* SurfacePadTexture(')
        destructor = (root / 'src/engine/vgui_EngineSurface.cpp').read_text()
    else:
        functions += function(surface, 'bool TextureBytes(')
        destructor = surface
    functions += function(destructor, 'EngineSurface::~EngineSurface(')
    for name in ('void EngineSurface::drawSetTextureRGBA(', 'void EngineSurface::drawSetTextureBGRA(',
                 'void EngineSurface::drawSetTexture(', 'void EngineSurface::drawSetTextureFile('):
        functions += function(surface, name)
    if 'void EngineSurface::refreshFileTextures(' in surface:
        functions += function(surface, 'void EngineSurface::refreshFileTextures(')
    functions += function((root / 'src/engine/vgui2/BaseUISurface.cpp').read_text(), 'bool BaseUISurface::DeleteTextureByID(')
    (out / f'{renderer}_surface.inc').write_text(functions)
    output = out / f'{renderer}_tests'
    command = ['c++', '-std=c++14', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
               '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer',
               f'-DTEST_{renderer.upper()}', '-I', str(out), '-I', str(root / 'src/engine'),
               '-I', str(root / 'src/common'), '-I', str(args.webp_include.resolve()),
               str(root / 'tests/native/image_consistency/tests.cpp'), str(root / 'src/common/image_codec.cpp'),
               str(args.webp_library.resolve()), '-o', str(output)]
    subprocess.run(command, check=True)
    result = subprocess.run([str(output)], text=True, capture_output=True)
    (out / f'{renderer}.log').write_text(result.stdout + result.stderr)
    print(result.stdout + result.stderr, end='')
    failures += result.returncode != 0
raise SystemExit(bool(failures))
