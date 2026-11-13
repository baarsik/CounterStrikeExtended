#!/usr/bin/env python3
"""Fixture-only definitions for declared unrelated methods. No class/layout copies."""
from pathlib import Path
import re
import sys

root, destination = map(Path, sys.argv[1:])
source = root / 'src'
panel = source / 'utils/vgui/include/VGUI_Panel.h'
wrap = source / 'engine/VGUI_EngineSurfaceWrap.h'
def unnamed(arguments):
    if arguments == '()':
        return arguments
    return '(' + ', '.join(re.sub(r'\b[A-Za-z_]\w*(?:\[\d*\])?$', '', arg.strip()).strip()
                           for arg in arguments[1:-1].split(',')) + ')'


text = ['#include "VGUI_EngineSurfaceWrap.h"', '#include "VGUI_Panel.h"', '#include <cstdlib>',
        'namespace vgui {', 'Panel::Panel(int, int, int, int) {', '    std::abort();', '}']
for declaration in re.findall(r'virtual\s+([^;{}]+);', panel.read_text(encoding='cp1252')):
    match = re.fullmatch(r'(.+?)\s+(\w+)\s*(\(.*\))', ' '.join(declaration.split()))
    if match:
        text.extend([match[1] + ' Panel::' + match[2] + unnamed(match[3]) + ' {', '    std::abort();', '}'])
text += ['}', 'namespace {', '[[noreturn]] vgui::Panel* AbortSurfaceConstruction() {', '    std::abort();', '}', '}',
         'EngineSurfaceWrap::EngineSurfaceWrap(vgui::Panel*, IEngineSurface*)',
         '    : vgui::SurfaceBase(AbortSurfaceConstruction()) {', '    std::abort();', '}',
         'EngineSurfaceWrap::~EngineSurfaceWrap() {',
         '    (void) &_mouseBitmap;', '    (void) &_engineSurface;', '    (void) &_cursorLocked;',
         '    std::abort();', '}']
for declaration in re.findall(r'^\s*(?:virtual\s+)?([^;{}]+\([^;{}]*\)\s*(?:override)?);', wrap.read_text(), re.M):
    declaration = re.sub(r'\b(public|protected|private):', '', declaration)
    declaration = ' '.join(declaration.replace('override', '').split())
    match = re.fullmatch(r'(.+?)\s+(\w+)\s*(\(.*\))', declaration)
    if match and match[2] != 'EngineSurfaceWrap':
        text.extend([match[1] + ' EngineSurfaceWrap::' + match[2] + unnamed(match[3]) + ' {',
                     '    std::abort();', '}'])
# Never reached: AbortSurfaceConstruction prevents real or substitute Surface
# construction. Link-time type checking still uses its complete real declaration.
text += ['namespace vgui {', 'SurfaceBase::SurfaceBase(Panel*) {', '    std::abort();', '}',
         'SurfaceBase::~SurfaceBase() {', '    std::abort();', '}', '}']
text.append('namespace vgui {')
for declaration in re.findall(r'virtual\s+([^;{}]+);',
        (source / 'utils/vgui/include/VGUI_SurfaceBase.h').read_text(encoding='cp1252')):
    if '=0' in declaration.replace(' ', '') or '~SurfaceBase' in declaration:
        continue
    match = re.fullmatch(r'(.+?)\s+(\w+)\s*(\(.*\))', ' '.join(declaration.split()))
    if match:
        text.extend([match[1] + ' SurfaceBase::' + match[2] + unnamed(match[3]) + ' {',
                     '    std::abort();', '}'])
text.append('}')
destination.write_text('\n'.join(text) + '\n')
