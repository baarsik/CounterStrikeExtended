#!/usr/bin/env python3
"""Import exact selected scalar bodies; no renderer/window/provider execution."""
import argparse
import hashlib
import json
from pathlib import Path

def function(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 1
    index = brace + 1
    while depth:
        depth += (text[index] == '{') - (text[index] == '}')
        index += 1
    return text[start:index]

def branch(body, windows):
    if '#if defined( _WIN32 )' not in body:
        return body
    before, rest = body.split('#if defined( _WIN32 )', 1)
    yes, rest = rest.split('#else', 1)
    no, after = rest.split('#endif', 1)
    return before + (yes if windows else no) + after

def generate(source, output, windows, baseline_types=False):
    source = source.resolve()
    text = source.read_text()
    output.mkdir(parents=True, exist_ok=True)
    signatures = [
        'float RectCoordinateDifference(', 'bool ScissorRect_TCoords(',
        'bool SurfaceIntersectRect( RECT* dest, const RECT* src1, const RECT* src2 )\n{',
        'bool ScissorRect( int x0, int y0, int x1, int y1, RECT* pOut )',
    ]
    native_difference = 'float RectCoordinateDifference(std::int32_t'
    if not windows and native_difference in text:
        signatures[0] = native_difference
    bodies = {signature: function(text, signature) for signature in signatures}
    bodies[signatures[2]] = branch(bodies[signatures[2]], windows)
    if not windows:
        bodies['bool SurfaceRectEmpty('] = function(text, 'bool SurfaceRectEmpty(')
        bodies[signatures[0]] = branch(bodies[signatures[0]], False)
    if windows:
        bodies[signatures[0]] = branch(bodies[signatures[0]], True)
    struct = text[text.index('struct TCoordRect'):text.index('bool g_bScissor;')]
    prefix = '#include <cstdint>\n'
    if windows or baseline_types:
        prefix += '#include <windows.h>\n'
    else:
        prefix += '#include "cse_rect.h"\n'
    # Windows-only baseline comparison deliberately uses real SDK LONG/ULONG/RECT,
    # preserving the prior native algorithm with its exact legacy32 data widths.
    code = prefix + struct + '\nbool g_bScissor;\nRECT g_ScissorRect;\n'
    code += '\nbool SurfaceIntersectRect(RECT*, const RECT*, const RECT*);\n'
    if not windows:
        code += '\n' + bodies['bool SurfaceRectEmpty('] + '\n'
    for signature in signatures:
        code += '\n' + bodies[signature] + '\n'
    (output / 'imported_surface.cpp').write_text(code)
    screen = source.with_name('gl_screen.cpp')
    screen_text = function(screen.read_text(), 'void SCR_ScreenShot_f()')
    dimensions = screen_text[screen_text.index('\tint width;'):screen_text.index('\n\tchar tgaName')]
    screen_code = prefix + '#include "vid.h"\nRECT window_rect;\nviddef_t vid{};\nint bDoScaledFBO;\n'
    screen_code += 'int windowedResult;\nint windowedCalls;\nextern "C" int VideoMode_IsWindowed() { ++windowedCalls; return windowedResult; }\n'
    screen_code += 'void ImportedScreenDimensions(int& outputWidth, int& outputHeight) {\n' + dimensions
    screen_code += '\noutputWidth = width;\noutputHeight = height;\n}\n'
    (output / 'imported_screen.cpp').write_text(screen_code)
    (output / 'imports.json').write_text(json.dumps({'source':str(source),
        'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(), 'windows_branch':windows,
        'screen_source':str(screen),'screen_source_sha256':hashlib.sha256(screen.read_bytes()).hexdigest(),
        'screen_dimensions_sha256':hashlib.sha256(dimensions.encode()).hexdigest(),
        'controlled_baseline_sdk_types':baseline_types,
        'bodies':{name:hashlib.sha256(body.encode()).hexdigest() for name,body in bodies.items()}},indent=2)+'\n')
    return struct

if __name__ == '__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--source',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--windows',action='store_true')
    parser.add_argument('--baseline-sdk-types',action='store_true')
    args=parser.parse_args()
    generate(args.source,args.output,args.windows,args.baseline_sdk_types)
