"""Compile exact owning GameUI/host/registry bodies without starting Steam or a GUI."""
import hashlib
import json
import re
from pathlib import Path
import sys

root = Path(sys.argv[1])
out = Path(sys.argv[2])
out.mkdir(exist_ok=True)
inputs = {}


def read(name):
    path = root / 'src' / name
    inputs[name] = hashlib.sha256(path.read_bytes()).hexdigest()
    return path.read_text(encoding='cp1252')


def body(text, signature):
    match = re.search(re.escape(signature) + r"[^;{]*\{", text)
    assert match, signature
    start = match.start()
    first = match.end() - 1
    depth = 1
    end = first + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


host = read('engine/host_cmd.cpp')
parts = []
if 'void Host_ResetNativeVideoRestart()' in host:
    parts += ['cse_restart::VideoSettings nativeVideoRestart;', body(host, 'void Host_ResetNativeVideoRestart()')]
for name in ['Host_SetRenderer_f', 'Host_SetVideoMode_f', 'Host_Quit_Restart_f']:
    parts.append(body(host, 'void ' + name + '()'))
(out / 'menu_host.inc').write_text('\n\n'.join(parts) + '\n')
menu = read('gameui/OptionsSubVideo.cpp')
(out / 'menu_gameui.inc').write_text('\n\n'.join(body(menu, signature) for signature in [
    'void COptionsSubVideo::ApplyVidSettings( bool forceRefresh )',
    'void COptionsSubVideo::SetCurrentRendererComboItem()']) + '\n')
video = read('engine/sys_getmodes.cpp')
(out / 'menu_video.inc').write_text('\n\n'.join(body(video, 'extern "C" void ' + name + '(' + args)
    for name, args in [('VideoMode_SetVideoMode', 'int width'), ('VideoMode_SwitchMode', 'int hardware')]) + '\n')
hardware_report = video[video.rindex('void VideoMode_GetCurrentRenderer('):]
(out / 'menu_renderer_report.inc').write_text(body(hardware_report, 'void VideoMode_GetCurrentRenderer(') + '\n')
registry = read('engine/registry.cpp')
start = registry.index('class CRegistry', registry.index('\n#else\n'))
end = registry.index('\n#endif // POSIX', start)
(out / 'menu_registry.inc').write_text(registry[start:end] + '\n')
lifecycle = read('engine/host.cpp')
if 'void Host_ResetNativeVideoRestart()' in host:
    initialize = body(lifecycle, 'int Host_Init( quakeparms_t* parms )')
    shutdown = body(lifecycle, 'void Host_Shutdown()')
    assert initialize.index('Host_ResetNativeVideoRestart();') < initialize.index('Memory_Init(')
    assert shutdown.index('Host_ResetNativeVideoRestart();') < shutdown.index('Host_WriteConfiguration();')

# Preserve the whole real entry/session owner, replacing only the game Run call
# so the finished engine's real constructors/root admission can be tested without
# initializing Steam, a game, audio or shared UI. No admission outcome is stubbed.
launcher = read('launcher/launcher.cpp')
assert launcher.count('engine->Run(') == 2
declaration = '''#include "public/engine_launcher_api.h"
int FixtureStopBeforeGame(IEngineAPI*, void*, char*, const char*, char*, CreateInterfaceFn, CreateInterfaceFn);
'''
(out / 'runtime_launcher.cpp').write_text(declaration + launcher.replace('engine->Run(', 'FixtureStopBeforeGame(engine, '))
(out / 'menu-source-identities.json').write_text(json.dumps(inputs, indent=2) + '\n')
