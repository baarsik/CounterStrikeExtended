#!/usr/bin/env python3
"""Exercise actual command filtering and multiplayer-option owners without running a game."""
import argparse
import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser()
parser.add_argument('--output', type=Path, default=ROOT / 'build/slowhacking-filter-tests')
parser.add_argument('--only', choices=('engine', 'ui'))
args = parser.parse_args()
OUT = args.output.resolve()
OUT.mkdir(parents=True, exist_ok=True)
HERE = Path(__file__).resolve().parent


def source(relative):
    return (ROOT / 'src' / relative).read_text(encoding='cp1252')


def function(relative, signature):
    text = source(relative)
    start = text.index(signature)
    end = text.index('\n}', text.index('{', start)) + 2
    return text[start:end] + '\n'


def run(name, parts):
    (OUT / ('actual_' + name + '.inc')).write_text('\n'.join(parts))
    binary = OUT / name
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++17', '-g', '-O1',
                    '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    '-Wno-writable-strings', '-Wno-deprecated-declarations', '-I' + str(ROOT / 'src'),
                    '-I' + str(ROOT / 'src/GameUI'), '-I' + str(OUT), '-I' + str(HERE),
                    str(HERE / (name + '_tests.cpp')), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)


if args.only != 'ui':
    parts = ['#include "engine_services.h"']
    for file, names in (
        ('engine/unicode_strtools.cpp', ['Q_IsValidUChar32', 'Q_UTF8ToUChar32', 'V_UTF8ToUChar32', 'Q_UnicodeAdvance']),
        ('engine/common.cpp', ['COM_Parse']),
        ('engine/sv_main.cpp', ['Q_stristr']),
        ('engine/cvar.cpp', ['Cvar_FindVar']),
        ('engine/cmd.cpp', ['Cbuf_AddTextToBuffer', 'Cbuf_AddTextToUntrustedBuffer', 'Cbuf_InsertTextToBuffer',
                            'Cbuf_ExecuteCommandsFromBuffer', 'Cmd_TokenizeString', 'Cmd_ExecuteStringWithPrivilegeCheck']),
        ('engine/cvar.cpp', ['Cvar_IsMultipleTokens', 'Cvar_CommandWithPrivilegeCheck'])):
        for name in names:
            match = re.search(r'^[^\n]*\b' + name + r'\([^\n]*\)\n\{', source(file), re.M)
            parts.append(function(file, match.group()))
    parts.append(re.search(r'^cvar_t cl_filterstuffcmd\s*=.*?;', source('engine/cl_main.cpp'), re.M).group())
    for name in ('StuffTextIsSpace', 'StuffTextStartsCommand', 'StuffTextHasCommand',
                 'StuffTextHasArgumentCommand', 'ValidStuffText', 'AllowedCommand'):
        match = re.search(r'^[^\n]*\b' + name + r'\([^\n]*\)\n\{', source('engine/cl_parsefn.cpp'), re.M)
        parts.append(function('engine/cl_parsefn.cpp', match.group()))
    run('engine', parts)

if args.only != 'engine':
    # These are external filesystem/widget services, not replacement option logic.
    (OUT / 'Panel.h').write_text('''#pragma once
namespace vgui2 { class Panel { public: virtual ~Panel() = default;
virtual void OnSizeChanged(int, int) {} }; }
''')
    (OUT / 'filesystem.h').write_text('#pragma once\nusing FileHandle_t = void*;\n')
    parts = ['#include "ui_services.h"']
    text = source('GameUI/MultiplayerAdvancedDialog.cpp')
    start = text.index('class CInfoDescription :')
    parts.append(text[start:text.index('\n};', start) + 3])
    for signature in ('CScriptListItem::CScriptListItem( const char*', 'CScriptObject::CScriptObject()',
                      'CScriptObject::~CScriptObject()', 'void CScriptObject::AddItem(',
                      'void CScriptObject::SetCurValue(', 'void StripFloatTrailingZeros(',
                      'void UTIL_StripInvalidCharacters(', 'void CScriptObject::WriteToConfig(',
                      'void CDescription::AddObject(', 'void CDescription::TransferCurrentValues('):
        parts.append(function('GameUI/ScriptObject.cpp', signature))
    parts.append('CInfoDescription::CInfoDescription(CPanelListPanel* panel) : CDescription(panel) {}\n'
                 'void CInfoDescription::WriteFileHeader(FileHandle_t) {}\n'
                 'void CInfoDescription::WriteScriptHeader(FileHandle_t) {}\n')
    parts.append(function('GameUI/MultiplayerAdvancedDialog.cpp', 'bool CInfoDescription::InitFromFile('))
    run('ui', parts)
