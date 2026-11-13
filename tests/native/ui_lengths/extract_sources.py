"""Extract the production HUD bodies without linking unrelated game/renderer code."""
import hashlib
import json
from pathlib import Path
import sys


def function(text, signature):
    start = text.index(signature)
    cursor = text.index('{', start)
    depth = 1
    end = cursor + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return start, text[start:end]


root = Path(sys.argv[1]).resolve()
output = Path(sys.argv[2]).resolve()
blocks = []
receipt = []


def add(relative, select, prefix='', suffix=''):
    path = root / relative
    data = path.read_bytes()
    text = data.decode('latin1')
    start, body = select(text)
    line = text.count('\n', 0, start) + 1
    blocks.append(f'{prefix}\n#line {line} "{path.as_posix()}"\n{body}\n{suffix}\n')
    receipt.append({'file': relative, 'source_sha256': hashlib.sha256(data).hexdigest(),
                    'line': line, 'body_sha256': hashlib.sha256(body.encode('latin1')).hexdigest()})


add('src/cstrike/cl_dll/hud.cpp', lambda text: function(text, 'void COM_FileBase('))
add('src/cstrike/cl_dll/hud_vgui2print.cpp',
    lambda text: function(text, 'void CHudVGUI2Print::GetStringSize('))


def chat(text):
    start = text.index('\twchar_t finalBuffer[256]', text.index('void CHudSayText::SayTextPrint('))
    end = text.index('\tchar* args[4]', start)
    return start, text[start:end]


add('src/cstrike/cl_dll/saytext.cpp', chat, 'std::wstring PrepareChat(const char* text) {',
    '(void)finalBuffer; return localized; }')


def spectator(text):
    start = text.index('        snprintf(string,', text.index('int CHudSpectator::Draw('))
    end = text.index('\n\n', text.index('lx = ', start))
    return start, text[start:end]


add('src/cstrike/cl_dll/hud_spectator.cpp', spectator,
    'int SpectatorOffset() { int i = 0; int lx = 0; char string[256];', 'return lx; }')
for signature in ('CUtlBuffer::CUtlBuffer( int', 'CUtlBuffer::CUtlBuffer( const void*',
                  'void CUtlBuffer::Get(', 'void CUtlBuffer::EatWhiteSpace(',
                  'void CUtlBuffer::GetString(', 'void CUtlBuffer::SeekGet(',
                  'void CUtlBuffer::Put(', 'void CUtlBuffer::PutString(',
                  'bool CUtlBuffer::CheckPut(', 'void CUtlBuffer::SeekPut('):
    add('src/vgui2/src/public/UtlBuffer.cpp', lambda text, key=signature: function(text, key))

output.mkdir(parents=True, exist_ok=True)
(output / 'hud_bodies.inc').write_text('\n'.join(blocks))
(output / 'extracted-bodies.json').write_text(json.dumps(receipt, indent=2) + '\n')
