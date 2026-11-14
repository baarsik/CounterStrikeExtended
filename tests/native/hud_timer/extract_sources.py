"""Extract complete production timer/font/draw bodies; substitute only external UI bindings."""
import hashlib
import json
from pathlib import Path
import sys


def block(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


root = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2])
out.mkdir(parents=True, exist_ok=True)
inputs = {
    'health': 'src/cstrike/cl_dll/health.cpp',
    'classes': 'src/cstrike/cl_dll/health.h',
    'message': 'src/cstrike/cl_dll/message.cpp',
    'hud': 'src/cstrike/cl_dll/hud.cpp',
    'redraw': 'src/cstrike/cl_dll/hud_redraw.cpp',
    'engine': 'src/engine/vgui2/text_draw.cpp',
    'policy': 'src/cstrike/cl_dll/cl_util.h',
}
sources = {key: (root / path).read_text(encoding='latin1') for key, path in inputs.items()}
classes = '\n'.join(block(sources['classes'], 'class ' + name + ' :') + ';' for name in
                    ['CHudRoundTimer', 'CHudCareerTaskTimer'])
(out / 'classes.inc').write_text(classes)
bodies = []
for name in ['CHudRoundTimer', 'CHudCareerTaskTimer']:
    for method in ['Draw(float time)', 'Reset()', 'VidInit()']:
        prefix = 'void ' if method == 'Reset()' else 'int '
        bodies.append(block(sources['health'], prefix + name + '::' + method))
for key, signature in [
    ('message', 'int CHudMessage::VidInit()'),
    ('redraw', 'void ScaleColors('),
    ('redraw', 'int CHud::DrawHudString('),
    ('engine', 'static bool VGUI2_IsDrawableCharacter('),
    ('engine', 'void VGUI2_Draw_Init()'),
    ('engine', 'int VGUI2_Draw_CharacterAdd('),
    ('engine', 'int VGUI2_DrawStringClient('),
]:
    body = block(sources[key], signature)
    if signature == 'void ScaleColors(':
        # Both actual owners include cl_util.h before this unchanged legacy body.
        # Limit their diagnostic context to the extracted body, not new fixture code.
        warnings = []
        for warning in (4244, 4305):
            line_number, pragma = next(
                (number, line) for number, line in enumerate(sources['policy'].splitlines(), 1)
                if line.strip() == f'#pragma warning( disable: {warning} )')
            warnings.append(f'// From {inputs["policy"]}:{line_number}\n{pragma}')
        scoped_body = ('#if defined(_MSC_VER)\n#pragma warning(push)\n'
                       + '\n'.join(warnings) + '\n#endif\n' + body
                       + '\n#if defined(_MSC_VER)\n#pragma warning(pop)\n#endif\n')
        (out / 'scale_colors.inc').write_text(scoped_body)
        bodies.append('#include "scale_colors.inc"')
    else:
        bodies.append(body)
(out / 'bodies.inc').write_text('\n\n'.join(bodies))
# Retain the owning declaration and the exact metric assignment, rather than modeling them.
declarations = []
for name in ['_font', '_Newfont']:
    declarations.append(next(line for line in sources['message'].splitlines()
                             if line == 'vgui2::HFont ' + name + ';'))
(out / 'fonts.inc').write_text('\n'.join(declarations))
metric = next(line for line in sources['hud'].splitlines() if 'm_iFontEngineHeight =' in line)
(out / 'metrics.inc').write_text('void CHud::MeasureLayoutFont() {\n' + metric + '\n}\n')
(out / 'source-manifest.json').write_text(json.dumps({
    path: hashlib.sha256((root / path).read_bytes()).hexdigest() for path in inputs.values()
}, indent=2) + '\n')
