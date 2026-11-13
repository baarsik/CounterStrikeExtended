"""Extract actual Half-Life HUD length owners for bounded, headless checks."""
import hashlib
import json
from pathlib import Path
import sys

root = Path(sys.argv[1]).resolve()
output = Path(sys.argv[2]).resolve()
blocks = []
receipt = []


def function(text, signature):
    begin = text.index(signature)
    cursor = text.index('{', begin)
    depth = 1
    end = cursor + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return begin, text[begin:end]


def add(name, select, prefix='', suffix=''):
    path = root / 'src/cl_dll' / name
    data = path.read_bytes()
    text = data.decode('latin1')
    start, body = select(text)
    line = text.count('\n', 0, start) + 1
    blocks.append(f'{prefix}\n#line {line} "{path.as_posix()}"\n{body}\n{suffix}\n')
    receipt.append({'file': str(path.relative_to(root)), 'source_sha256': hashlib.sha256(data).hexdigest(),
                    'line': line, 'body_sha256': hashlib.sha256(body.encode('latin1')).hexdigest()})


add('hud.cpp', lambda text: function(text, 'void COM_FileBase'))
add('text_message.cpp', lambda text: function(text, 'void StripEndNewlineFromString'))
add('hud_servers.cpp', lambda text: function(text, 'void CHudServers::ServerResponse'))


def spectator_name(text):
    begin = text.index('// draw the players name and health underneath', text.index('int CHudSpectator::Draw('))
    end = text.index('gEngfuncs.pfnDrawSetTextColor', begin)
    return begin, text[begin:end]


add('hud_spectator.cpp', spectator_name,
    'NameResult SpectatorName(const char* name) { int i = 0; int lx = 0; char string[256]; '
    'g_PlayerInfoList[1].name = name;', 'return {string, lx}; }')


def mode_message(text):
    begin = text.index('char string[128];', text.index('void CHudSpectator::SetModes('))
    call = text.index('gHUD.m_TextMessage.MsgFunc_TextMsg(', begin)
    end = text.index(';', call) + 1
    return begin, text[begin:end]


add('hud_spectator.cpp', mode_message, 'void ModeMessage(int mode) { g_iUser1 = mode;', '}')
output.mkdir(parents=True, exist_ok=True)
(output / 'hl_bodies.inc').write_text('\n'.join(blocks))
(output / 'hl-extracted-bodies.json').write_text(json.dumps(receipt, indent=2) + '\n')
