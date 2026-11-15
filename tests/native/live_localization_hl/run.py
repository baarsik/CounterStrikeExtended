#!/usr/bin/env python3
"""Exercise actual HL presentation owners under ASan/UBSan, with optional baseline replay."""
import argparse
import importlib.util
import json
import re
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--root', type=Path, default=Path('.'))
parser.add_argument('--evidence', type=Path, required=True)
parser.add_argument('--baseline', action='store_true')
parser.add_argument('--case', choices=('messages', 'deadline', 'menu', 'views', 'center', 'chat', 'all'), default='all')
args = parser.parse_args()
root = args.root.resolve()
out = args.evidence.resolve()
out.mkdir(parents=True, exist_ok=True)
spec = importlib.util.spec_from_file_location('extract', root / 'tests/native/network_platform/extract_functions.py')
extract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(extract)

def read(path):
    if args.baseline:
        return subprocess.check_output(['git', 'show', '538661e0ef6e5d0384bff9f85d0aa451cb93c09e:' + path], cwd=root).decode('latin1')
    return (root / path).read_bytes().decode('latin1')

def body(source, name):
    start = source.rfind('\n', 0, source.index(name)) + 1
    opening = source.index('{', start)
    depth = 0
    pattern = r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|[{}]'
    for token in re.finditer(pattern, source[opening:], re.S):
        if token[0] == '{':
            depth += 1
        elif token[0] == '}':
            depth -= 1
            if depth == 0:
                return source[start:opening + token.end()]
    raise ValueError(name)

def run(name):
    command = ['c++', '-std=c++17', '-g', '-O1', '-Wno-writable-strings', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
               '-I', str(out), '-I', str(root / 'src/public'), '-I', str(root / 'src/common'),
               '-I', str(root / 'tests/native/live_localization_hl/stubs'), '-I', str(root / 'src/cl_dll'),
               str(root / f'tests/native/live_localization_hl/{name}_tests.cpp'), '-o', str(out / f'{name}_tests')]
    (out / f'{name}-compile.json').write_text(json.dumps(command, indent=2))
    subprocess.run(command, check=True)
    subprocess.run([str(out / f'{name}_tests'), str(root)], check=True)

if args.case in ('messages', 'deadline', 'all'):
    text = read('src/cl_dll/legacy_message_text.h')
    (out / 'deadline-text.inc').write_text(body(text, 'inline bool IsBmpUtf8(') + '\n'
        + body(text, 'struct Text {') + ';\n')

if args.case in ('messages', 'all'):
    source = read('src/cl_dll/message.cpp')
    parts = []
    for name in ('CHudMessage::MessageAdd( const char', 'CHudMessage::MessageAdd(client_textmessage_t'):
        start = source.rfind('\n', 0, source.index(name)) + 1
        tail = source[start:]
        parts.append(tail[:tail.index('\n}') + 2])
    parts.append(extract.extract(source, 'CHudMessage::RefreshLanguage') if 'CHudMessage::RefreshLanguage' in source
                 else 'void CHudMessage::RefreshLanguage() {}')
    for name in ('CHudMessage::MessageEndTime', 'CHudMessage::ReleaseMessage'):
        if name in source:
            parts.append(body(source, name))
    (out / 'messages.inc').write_text('\n'.join(parts))
    run('message')

if args.case in ('deadline', 'all'):
    source = read('src/cl_dll/message.cpp')
    parts = [body(source, name) for name in ('CHudMessage::MessageAdd( const char',
        'CHudMessage::MessageAdd(client_textmessage_t', 'CHudMessage::RefreshLanguage',
        'CHudMessage::Draw(', 'CHudMessage::Reset(')]
    for name in ('CHudMessage::MessageEndTime', 'CHudMessage::ReleaseMessage'):
        if name in source:
            parts.append(body(source, name))
    (out / 'deadline.inc').write_text('\n'.join(parts))
    text = read('src/cl_dll/legacy_message_text.h')
    (out / 'deadline-text.inc').write_text(body(text, 'inline bool IsBmpUtf8(') + '\n'
        + body(text, 'struct Text {') + ';\n')
    run('deadline')

if args.case in ('menu', 'views', 'all'):
    text = read('src/cl_dll/text_message.cpp')
    (out / 'localize.inc').write_text('\n'.join(extract.extract(text, name) for name in
        ('LocalizedPrefixBytes', 'CHudTextMessage::LocaliseTextString')))

if args.case in ('menu', 'all'):
    menu = read('src/cl_dll/menu.cpp').replace('CHudMenu :: ', 'CHudMenu::')
    parts = [extract.extract(menu, 'CHudMenu::MsgFunc_ShowMenu')]
    parts.append(extract.extract(menu, 'CHudMenu::RefreshLanguage') if 'CHudMenu::RefreshLanguage' in menu
                 else 'void CHudMenu::RefreshLanguage() {}')
    (out / 'menu-refresh.inc').write_text('\n'.join(parts))
    (out / 'bindings.inc').write_text(extract.extract(read('src/cl_dll/input.cpp'), 'KB_ConvertString') + '\n' + extract.extract((root / 'src/cl_dll/localization_client.cpp').read_text(), 'FinishHudText'))
    run('menu')

if args.case in ('views', 'all'):
    source = read('src/cl_dll/vgui_language.cpp')
    source = '\n'.join(line for line in source.splitlines() if not line.startswith('#include'))
    (out / 'views.inc').write_text(source)
    schemes = read('src/game_shared/vgui_SchemeManager.cpp')
    (out / 'schemes.inc').write_text(extract.extract(schemes, 'CSchemeManager::ReloadLanguage'))
    run('view')

if args.case in ('center', 'all'):
    source = read('src/cl_dll/text_message.cpp').replace('char *CHudTextMessage::', 'char* CHudTextMessage::')
    names = ('CHudTextMessage::LookupString', 'StripEndNewlineFromString', 'ConvertCRtoNL',
             'CHudTextMessage::MsgFunc_TextMsg')
    parts = [extract.extract(source, name) for name in names]
    parts.append(extract.extract((root / 'src/cl_dll/localization_client.cpp').read_text(), 'FinishHudText'))
    parts.append(extract.extract(read('src/cl_dll/localization_client.cpp'), 'HudTextSource::Capture') if not args.baseline else 'bool HudTextSource::Capture(const std::string*, const char* const*) { return false; }')
    parts.append(extract.extract(source, 'CHudTextMessage::RefreshLanguage')
                 if 'CHudTextMessage::RefreshLanguage' in source else 'void CHudTextMessage::RefreshLanguage() {}')
    parts.append(body(read('src/cl_dll/localization_client.cpp'), 'HudTextSource::Render'))
    if 'CHudTextMessage::PrintLocalVoiceStatus' in source:
        parts.append(body(source, 'CHudTextMessage::PrintLocalVoiceStatus'))
    (out / 'center.inc').write_text('\n'.join(parts))
    producers = []
    for path, method, start, declaration, name in (
        ('vgui_SpectatorPanel.cpp', 'SpectatorPanel::ShowMenu', r'char string\[', '', 'LocalDuckOnHide'),
        ('vgui_TeamFortressViewport.cpp', 'TeamFortressViewport::UpdateSpectatorPanel',
         r'_?snprintf\(\s*tempString,', 'char tempString[128];', 'LocalDuckOnJoin'),
        ('hud_spectator.cpp', 'CHudSpectator::SetModes', r'char string\[', '', 'LocalSpectatorMode'),
    ):
        owner = body(read('src/cl_dll/' + path), method)
        begin = re.search(start, owner).start()
        end = owner.index(';', owner.index('gHUD.m_TextMessage.MsgFunc_TextMsg', begin)) + 1
        producers.append('void ' + name + '() {\n' + declaration + '\n' + owner[begin:end] + '\n}')
    producers.append(body(read('src/cl_dll/vgui_ScorePanel.cpp'), 'ScorePanel::mousePressed'))
    (out / 'local-notifications.inc').write_text('\n'.join(producers))
    run('center')

if args.case in ('chat', 'all'):
    source = read('src/cl_dll/saytext.cpp')
    helper = (root / 'src/cl_dll/localization_client.cpp').read_text()
    parts = [extract.extract(helper, 'HudTextSource::Capture'), body(helper, 'HudTextSource::Render'), extract.extract(helper, 'FinishHudText')]
    parts.append(extract.extract(source, 'ScrollTextUp'))
    if 'WrapLocalizedSayText' in source:
        parts.append(body(source, 'WrapLocalizedSayText'))
        parts.append(extract.extract(source, 'CHudSayText::RefreshLanguage'))
    else:
        parts.append('void CHudSayText::RefreshLanguage() {}')
    (out / 'chat.inc').write_text('\n'.join(parts))
    run('chat')
