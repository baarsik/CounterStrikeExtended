from pathlib import Path
import re
import sys


def function(source, name):
    match = re.search(r'^(?:bool|int)\s+' + re.escape(name) + r'\([^;]*?\)\s*\{', source, re.M)
    if not match:
        raise ValueError('Missing actual decoder ' + name)
    opening = source.index('{', match.start())
    position = opening + 1
    depth = 1
    while depth:
        depth += (source[position] == '{') - (source[position] == '}')
        position += 1
    return source[match.start():position]


sdk = Path(sys.argv[1])
output = Path(sys.argv[2])
source = (sdk / 'engine/vgui2/BaseUISurface.cpp').read_bytes().decode('cp1252')
start = source.index('    case SDL_EVENT_TEXT_INPUT:')
end = source.index('    case SDL_EVENT_MOUSE_MOTION:', start)
case = source[start:end]
decoder = (sdk / 'engine/unicode_strtools.cpp').read_bytes().decode('cp1252')
output.write_text('using uchar32 = std::uint32_t;\nusing byte = unsigned char;\n' +
    function(decoder, 'Q_IsValidUChar32') + '\n' + function(decoder, 'Q_UTF8ToUChar32') + '\n' +
    'void DispatchActualSdlText(SDL_Event& ev) {\n    switch (ev.type) {\n' + case +
    '    default: break;\n    }\n}\n')
