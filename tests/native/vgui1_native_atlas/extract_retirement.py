"""Exact private method/data spans; no replacement owner class declarations."""
import argparse
import hashlib
import json
import re
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--sdk', required=True, type=Path)
parser.add_argument('--output', required=True, type=Path)
args = parser.parse_args()
args.output.mkdir(exist_ok=True)


def body(text, pattern):
    match = re.search(pattern, text, re.M)
    assert match, pattern
    first = text.index('{', match.end())
    depth = 1
    end = first + 1
    while depth:
        if text[end] == '{':
            depth += 1
        elif text[end] == '}':
            depth -= 1
        end += 1
    return text[match.start():end]


hw = (args.sdk / 'engine/vgui_EngineSurfaceHW.cpp').read_text(encoding='cp1252')
start = hw.index('struct Texture\n')
end = hw.index('struct VertexBuffer_t', start)
parts = [hw[start:end], 'int g_iVertexBufferEntriesUsed;']
for name in ['VguiSurfaceTexturesLessFunc', 'staticGetTextureById', 'staticAllocTextureForId']:
    # Explicitly require the definition line (declaration may precede it).
    parts.append(body(hw, r'^static [^;\n]*\b' + name + r'\([^;\n]*\)\n(?=\{)'))
(args.output / 'hw_metadata.inc').write_text('\n\n'.join(parts) + '\n')
shared_retirement = (body(hw, r'^static bool RetireSurfaceTexture\([^\n]*\) noexcept')
                     if 'static bool RetireSurfaceTexture(' in hw else '')
parts = []
for name in ['UploadEngineFontPage', 'RetireEngineFontPage']:
    parts.append(body(hw, r'^bool ' + name + r'\([^\n]*\) noexcept'))
(args.output / 'hw_retirement.inc').write_text(shared_retirement + '\nnamespace cse_vgui1_native {\n' + '\n\n'.join(parts) + '\n}\n')

sw = (args.sdk / 'engine/vgui_EngineSurfaceSW.cpp').read_text(encoding='cp1252')
start = sw.index('#define TEXTURE_HASH_SIZE')
end = sw.index('\nint g_TranslateX', start)
parts = [sw[start:end], 'EngineSurface* EngineSurface::s_pEngineSurface = nullptr;']
helper_start = sw.index('namespace {\nconstexpr std::size_t TextureChannels')
helper_end = sw.index('\nvoid EngineSurfaceTextures::Insert', helper_start)
parts.append(sw[helper_start:helper_end])
for name in ['EngineSurfaceTextures', '~EngineSurfaceTextures', 'InitTextures', 'FreeTextures', 'GetHashVal',
             'Insert', 'Remove', 'GetTexture'] + (['RetireAtlasPage'] if '::RetireAtlasPage(' in sw else []):
    parts.append(body(sw, r'^(?:[^\n]*\s)?EngineSurfaceTextures::' + re.escape(name) + r'\([^\n]*\)'))
actual = ['EngineSurface', '~EngineSurface', 'createRenderPlat', 'deleteRenderPlat', 'RetireNativeFontPage',
          'drawSetTextureRGBA', 'drawSetTextureBGRA', 'drawSetTexture', 'isTextureIdValid']
if 'bool EngineSurface::deleteTexture(' in sw:
    actual.insert(actual.index('RetireNativeFontPage'), 'deleteTexture')
for name in actual:
    parts.append(body(sw, r'^(?:[^\n]*\s)?EngineSurface::' + re.escape(name) + r'\([^\n]*\)(?: noexcept)?'))
(args.output / 'sw_retirement.inc').write_text('\n\n'.join(parts) + '\n')
header = (args.sdk / 'engine/vgui_EngineSurface.h').read_text()
stubs = []
for match in re.finditer(r'^\t([^;{}]*?\([^;{}]*?\)) override;', header, re.M):
    signature = match.group(1).strip()
    name = signature.split('(')[0].split()[-1]
    if name in actual:
        continue
    prefix, parameters = signature.split('(', 1)
    names = []
    for parameter in parameters[:-1].split(','):
        if parameter.strip():
            names.append(re.search(r'(\w+)\s*$', parameter).group(1))
    prefix = prefix.rsplit(name, 1)[0] + 'EngineSurface::' + name
    statements = ['    (void)' + name + ';' for name in names]
    stubs.append(prefix + '(' + parameters + ' {\n' + '\n'.join(statements)
                 + '\n    Unexpected();\n}')
(args.output / 'sw_unrelated.inc').write_text('\n\n'.join(stubs) + '\n')

runtime = (args.sdk / 'engine/VGui1NativeRuntime.cpp').read_text()
(args.output / 'upload_fence.inc').write_text('namespace cse_vgui1_native {\n'
    + body(runtime, r'^bool UploadOwnedInterface\([^\n]*\n[^\n]*\) noexcept') + '\n}\n')

inputs = [args.sdk / 'engine' / name for name in ['vgui_EngineSurfaceHW.cpp', 'vgui_EngineSurfaceSW.cpp',
                                                               'vgui_EngineSurface.h', 'VGui1NativeRuntime.cpp']]
outputs = sorted(args.output.glob('*.inc'))
(args.output / 'extraction-identities.json').write_text(json.dumps({
    'inputs': {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in inputs},
    'outputs': {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in outputs}
}, indent=2) + '\n')
