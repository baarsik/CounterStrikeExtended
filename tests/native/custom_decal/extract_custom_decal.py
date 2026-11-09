"""Import complete production function bodies; surrounding OS/render services remain typed fixture seams."""
from pathlib import Path
import argparse
import hashlib
import json
import re


def functions(text, name):
    result = []
    for match in re.finditer(r'^[\w:*<> ]+\b' + re.escape(name) + r'\([^;]*?\)\s*\{', text, re.M):
        brace = text.index('{', match.start())
        depth = 0
        for end in range(brace, len(text)):
            if text[end] == '{':
                depth += 1
            elif text[end] == '}':
                depth -= 1
                if not depth:
                    result.append(text[match.start():end + 1])
                    break
    return result


parser = argparse.ArgumentParser()
parser.add_argument('--sdk', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
records = []


def source(relative):
    path = args.sdk / relative
    data = path.read_bytes()
    records.append(dict(relative=relative, sha256=hashlib.sha256(data).hexdigest()))
    return data.decode('utf-8').replace('\r\n', '\n')


common = source('engine/common.cpp')
start = common.index('\nenum\n{\n\tLOADFILE_ZONE') + 1
end = common.index('\nbyte* COM_LoadHunkFile', start)
(args.output / 'loader.inc').write_text(common[start:end] + '\n')
draw = source('engine/draw.cpp')
gl_draw = source('engine/gl_draw.cpp')
decals = source('engine/decals.cpp')
wad = source('engine/wad.cpp')
builders = '#if defined(SW_RENDERER)\n' + functions(draw, 'Draw_MiptexTexture')[0]
builders += '\n#else\n' + functions(gl_draw, 'Draw_MiptexTexture')[0] + '\n#endif\n'
(args.output / 'builders.inc').write_text(builders)
validators = functions(decals, 'Draw_ValidateCustomLogo')
assert len(validators) == 2
(args.output / 'logo_validation.inc').write_text('#if defined(SW_RENDERER)\n' + validators[0]
    + '\n#else\n' + validators[1] + '\n#endif\n')
custom_names = ['Draw_AllocateCacheSpace', 'Draw_CacheWadHandler', 'Draw_FreeWad', 'Draw_CustomCacheWadInit',
                'Draw_CacheByIndex', 'CustomDecal_Init', 'CustomDecal_Validate',
                'Draw_CacheLoadFromCustom', 'Draw_CustomCacheGet']
parts = ['#if defined(SW_RENDERER)\n']
for name in custom_names:
    bodies = functions(decals, name)
    assert len(bodies) == 2, (name, len(bodies))
    parts.append(bodies[0] + '\n')
parts.append('#else\n')
for name in custom_names:
    parts.append(functions(decals, name)[1] + '\n')
parts.append('#endif\n')
(args.output / 'custom_functions.inc').write_text(''.join(parts))
(args.output / 'names.inc').write_text(functions(wad, 'W_CleanupName')[0] + '\n'
    + functions(common, 'COM_FileBase_s')[0] + '\n')
(args.output / 'source-hashes.json').write_text(json.dumps(records, indent=2) + '\n')
