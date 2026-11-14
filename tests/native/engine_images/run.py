#!/usr/bin/env python3
"""Compile the real engine loaders with an in-memory filesystem and libwebp fixtures."""
import argparse
from pathlib import Path
import shlex
import subprocess
import sys

parser = argparse.ArgumentParser()
parser.add_argument('--root', type=Path, default=Path('.'))
parser.add_argument('--evidence', type=Path, required=True)
parser.add_argument('--webp-library', type=Path)
parser.add_argument('--webp-include', type=Path)
args = parser.parse_args()
root = args.root.resolve()
out = args.evidence.resolve()
out.mkdir(parents=True, exist_ok=True)


def function(source, signature):
    start = source.index(signature)
    position = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[position] == '{') - (source[position] == '}')
        position += 1
    return source[start:position]


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
end = hardware.index('static const char* suf[6]' if 'static const char* suf[6]' in hardware else 'static char* suf[6]', start)
(out / 'hardware.inc').write_text(hardware[start:end])
(out / 'sky.inc').write_text(hardware[end:hardware.index('void R_LoadSkys()', end)] + function(hardware, 'void R_LoadSkys('))
policy = (root / 'src/engine/image_consistency.cpp').read_text()
(out / 'image_policy.inc').write_text(policy[policy.index('namespace cse {'):])
map_source = (root / 'src/engine/gl_rmain.cpp').read_text()
(out / 'map.inc').write_text(function(map_source, 'model_t* R_LoadMapSprite('))
for renderer, suffix in (('hardware', 'HW'), ('software', 'SW')):
    surface = (root / ('src/engine/vgui_EngineSurface' + suffix + '.cpp')).read_text()
    (out / (renderer + '_surface.inc')).write_text(function(surface, 'void EngineSurface::drawSetTextureFile('))
software = (root / 'src/engine/r_sky.cpp').read_text()
software_signatures = ['short PutDitheredRGB(']
if 'static byte* R_CopyWebPImage(' in software:
    software_signatures.append('static byte* R_CopyWebPImage(')
if 'static byte* R_LoadTGAImage(' in software:
    software_signatures.append('static byte* R_LoadTGAImage(')
software_signatures += ['byte* R_LoadTGA(', 'qboolean LoadTGA(']
(out / 'software.inc').write_text('\n\n'.join(function(software, signature) for signature in software_signatures) + '\n')
software_sky = function(software, 'void R_LoadSkys(') + '\n' + function(software, 'void R_FilterSkyBox(')
draw = (root / 'src/engine/draw.cpp').read_text()
(out / 'software_sky.inc').write_text(function(draw, 'void GetRGB(') + '\n' + software_sky)
font = (root / 'src/engine/vgui2/BaseUISurface_Font.cpp').read_text()
font_header = font[font.index('#pragma pack('):font.index('class FontPlatDar')]
font_signatures = ['FileImageStream_Memory::FileImageStream_Memory(', 'void FileImageStream_Memory::Read(',
                   'bool FileImageStream_Memory::ErrorStatus(', 'VFontData::VFontData(', 'VFontData::~VFontData(',
                   'bool Load32BitTGA(', 'bool LoadVFontDataFrom32BitTGA(']
(out / 'font.inc').write_text(font_header + '\n'.join(function(font, signature) for signature in font_signatures))
if args.webp_library:
    if not args.webp_include:
        parser.error('--webp-include is required with --webp-library')
    flags = ['-I', str(args.webp_include.resolve()), str(args.webp_library.resolve()), '-pthread']
else:
    flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'libwebp'], text=True))
codec = root / 'src/common/image_codec.cpp'
failures = 0
for renderer in ('hardware', 'software'):
    command = ['c++', '-std=c++14', '-g', '-O1', '-Wall', '-Wextra', '-Werror',
               '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
               '-DTEST_' + renderer.upper(), '-I', str(out), '-I', str(root / 'src/common'),
               '-I', str(root / 'src/engine'), str(root / 'tests/native/engine_images/loader_tests.cpp')]
    if codec.exists():
        command.append(str(codec))
    command += flags + ['-o', str(out / (renderer + '_tests'))]
    subprocess.run(command, check=True)
    result = subprocess.run([str(out / (renderer + '_tests'))], check=False, capture_output=True, text=True)
    (out / (renderer + '.log')).write_text(result.stdout + result.stderr)
    sys.stdout.write(result.stdout)
    sys.stderr.write(result.stderr)
    failures += result.returncode != 0
raise SystemExit(bool(failures))
