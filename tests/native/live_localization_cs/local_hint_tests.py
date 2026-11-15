"""Run the real VGUI hint producer, client bridge, message owner and glyph renderer."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[3]
output = root / 'build/live-localization/cs-local-hint-tests'
output.mkdir(parents=True, exist_ok=True)


def function(source, signature):
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


message = (root / 'src/cstrike/cl_dll/message.cpp').read_text(encoding='cp1252')
signatures = [
    'int CHudMessage::MessageAdd( const char*',
    'void CHudMessage::MessageAdd( client_textmessage_t*',
    'float CHudMessage::MessageEndTime(',
    'void CHudMessage::RefreshLanguage(',
    'void CHudMessage::MessageDrawScan(',
    'void CHudMessage::MessageScanStart(',
    'void CHudMessage::MessageScanNextChar(',
    'int CHudMessage::XPosition(',
    'int CHudMessage::YPosition(',
]
for signature in ('void CHudMessage::MessageAddLocal(', 'void CHudMessage::QueueMessage('):
    if signature in message:
        signatures.append(signature)
bodies = [function(message, signature) for signature in signatures]
bridge = (root / 'src/cstrike/cl_dll/VGUI/counterstrikeviewport_interface.cpp').read_text(encoding='cp1252')
bodies.append(function(bridge, 'void MessageAdd( void* message )'))
producer = (root / 'src/cstrike/vgui2/game_controls/vgui_TeamFortressViewport_msgs.cpp').read_text(encoding='cp1252')
bodies.append(function(producer, 'void DuckMessage( const char* text )'))
(output / 'hint_bodies.inc').write_text('\n\n'.join(bodies))
executable = output / 'local_hints'
subprocess.run([
    'c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-Wno-sign-compare', '-fsanitize=address,undefined',
    '-I', str(root / 'src/public'), '-I', str(output),
    str(root / 'tests/native/live_localization_cs/local_hint_tests.cpp'), '-o', str(executable),
], check=True)
subprocess.run([str(executable)], check=True)
