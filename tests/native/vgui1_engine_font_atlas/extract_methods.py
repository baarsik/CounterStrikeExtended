"""Extract exact owned method/state spans; no replacement class declarations."""
import argparse
import hashlib
import json
from pathlib import Path
import re

parser = argparse.ArgumentParser()
parser.add_argument('--source', required=True, type=Path)
parser.add_argument('--output', required=True, type=Path)
parser.add_argument('--mutant', default='none', choices=['none', 'atlas-width', 'draw-width', 'admission', 'uv-scale'])
args = parser.parse_args()
raw = args.source.read_bytes()
text = raw.decode('cp1252')


def body(name):
    match = re.search(r'^.*\bEngineSurfaceWrap::' + name + r'\s*\(', text, re.M)
    assert match, name
    start = match.start()
    first = text.index('{', match.end())
    index = first
    depth = 0
    state = ''
    while index < len(text):
        value = text[index]
        following = text[index:index + 2]
        if state == '//':
            if value == '\n':
                state = ''
        elif state == '/*':
            if following == '*/':
                state = ''
                index += 1
        elif state in ['"', "'"]:
            if value == '\\':
                index += 1
            elif value == state:
                state = ''
        elif following in ['//', '/*']:
            state = following
            index += 1
        elif value in ['"', "'"]:
            state = value
        elif value == '{':
            depth += 1
        elif value == '}':
            depth -= 1
            if depth == 0:
                result = text[start:index + 1]
                return result, {'first-line': text.count('\n', 0, start) + 1,
                                'sha256-cp1252': hashlib.sha256(result.encode('cp1252')).hexdigest()}
        index += 1
    raise AssertionError(name)


names = ['drawSetTextFont', 'drawPrintText', 'createNewTextureID', 'drawSetTextureRGBA',
         'drawSetTexture', 'drawSetTextPos', 'drawGetTextPos', 'drawPrintChar', 'invalidate']
methods = {}
spans = {}
for name in names:
    methods[name], spans[name] = body(name)
class_start = text.index('class FontInfoVGUI\n')
class_end = text.index('};', class_start) + 2
state = [text[class_start:class_end]]
for prefix in ['constexpr int FONT_TEXTURE_SIZE', 'constexpr int FONT_TEXTURE_BYTES',
               'constexpr float FONT_TEXTURE_COORD_SCALE', 'constexpr int FONT_PAGE_LIMIT',
               'static_assert( sizeof( FontInfoVGUI )', 'static vgui::Font* staticFont ',
               'static FontInfoVGUI* staticFontInfoVGUI ', 'static vgui::Dar<FontInfoVGUI*> staticFontInfoVGUIDar',
               'static int staticContextCount ', 'static unsigned char staticRGBA[']:
    state.append(next(line for line in text.splitlines() if line.startswith(prefix)))
if args.mutant == 'atlas-width':
    methods['drawSetTextFont'] = methods['drawSetTextFont'].replace('const int wide = abcB;', 'const int wide = abcB + 1;')
elif args.mutant == 'draw-width':
    methods['drawPrintText'] = methods['drawPrintText'].replace('const int wide = abcB;', 'const int wide = abcB + 1;')
elif args.mutant == 'admission':
    needle = '\n\t\tint abcA = 0;'
    assert needle in methods['drawSetTextFont']
    methods['drawSetTextFont'] = methods['drawSetTextFont'].replace(needle,
        '\n\t\tif ( character == 0xe9 ) { ++character; continue; }' + needle, 1)
elif args.mutant == 'uv-scale':
    state = [line.replace('0.001953125f', '0.00390625f') for line in state]
args.output.mkdir(parents=True, exist_ok=True)
(args.output / 'atlas_state.inc').write_text('\n'.join(state) + '\n')
(args.output / 'atlas_methods.inc').write_text('\n\n'.join(methods.values()) + '\n')
(args.output / 'extraction.json').write_text(json.dumps({'source': str(args.source),
    'source-sha256': hashlib.sha256(raw).hexdigest(), 'methods': spans,
    'mutant': args.mutant, 'state-class-sha256': hashlib.sha256(text[class_start:class_end].encode()).hexdigest()},
    indent=2) + '\n')
