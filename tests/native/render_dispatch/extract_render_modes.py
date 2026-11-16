"""Compile production brush, studio, and sprite mode setup against native dispatch."""
from pathlib import Path
import re
import sys

root = Path(sys.argv[1])
output = Path(sys.argv[2])


def body(source, signature):
    match = re.search(re.escape(signature) + r"[^;{]*\{", source)
    assert match, signature
    end = match.end()
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end]


constants = (root / 'src/common/const.h').read_text(encoding='cp1252')
constants = constants[constants.index('// Rendering constants'):]
parts = [body(constants, 'enum') + ';']
for filename, signature in [('gl_rsurf.cpp', 'void R_SetRenderMode('),
                            ('r_studio.cpp', 'void studioapi_GL_SetRenderMode( int rendermode )')]:
    source = (root / 'src/engine' / filename).read_text(encoding='cp1252')
    parts.append(body(source, signature))

source = (root / 'src/engine/gl_rmain.cpp').read_text(encoding='cp1252')
sprite = body(source, 'void R_DrawSpriteModel(')
start = sprite.index('if ( gl_spriteblend.value ||')
end = sprite.index('// Get orthonormal bases', start)
parts.append('void SpriteRenderMode(cl_entity_t* e, const FixtureColor& color) {\n'
             + sprite[start:end] + '\n}')
output.write_text('\n\n'.join(parts) + '\n')
