from pathlib import Path
import hashlib
import json
import sys

root = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)
source = root / 'GoldSrc-SDK/engine/sys_sdlwind.cpp'
text = source.read_text()
pins = []

def body(start):
    opening = text.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end], end

assert text.count('void CGame::SleepUntilInput(') == 2
offset = 0
for name in ['sw', 'hw']:
    begin = text.index('void CGame::SleepUntilInput(', offset)
    value, offset = body(begin)
    (out / (name + '_loop.inc')).write_text(value)
    pins.append(dict(name=name, sha256=hashlib.sha256(value.encode()).hexdigest()))
value, _ = body(text.index('static int EngineKeyForSDLScancode('))
(out / 'key_mapping.inc').write_text(value)
needle = 'bool cse::RefreshNativeGameplayShortcuts()'
if needle in text:
    value, _ = body(text.index(needle))
    (out / 'context.inc').write_text(value)
else:
    (out / 'context.inc').write_text('// Baseline has no context refresh function.\n')
needle = 'bool cse::RefreshNativeMouseCapture()'
value, _ = body(text.index(needle))
(out / 'capture_context.inc').write_text(value)
(out / 'pins.json').write_text(json.dumps(dict(source=str(source), source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(), bodies=pins), indent=2)+'\n')

# Complete selected shutdown and listen-owner bodies, with original compile-time branches retained.
for relative, method, suffix in [
    ('GoldSrc-SDK/engine/sys_sdlwind.cpp', 'bool CGame::Shutdown()', 'shutdown'),
    ('GoldSrc-SDK/engine/sys_dll2.cpp', 'int RunListenServer(', 'listen'),
]:
    text = (root / relative).read_text()
    offset = 0
    for name in ['sw', 'hw']:
        begin = text.index(method, offset)
        value, offset = body(begin)
        (out / (name + '_' + suffix + '.inc')).write_text(value)
        pins.append(dict(source=relative, name=name + '-' + suffix,
                         sha256=hashlib.sha256(value.encode()).hexdigest()))
text = (root / 'GoldSrc-SDK/engine/sys_dll2.cpp').read_text()
begin = text.index('namespace {\nenum class NativeListenShutdownPhase')
value = text[begin:text.rindex('#endif')]
(out / 'shutdown_phases.inc').write_text(value)
pins.append(dict(source='GoldSrc-SDK/engine/sys_dll2.cpp', name='shutdown-phases',
                 sha256=hashlib.sha256(value.encode()).hexdigest()))
(out / 'lifecycle-pins.json').write_text(json.dumps(pins, indent=2) + '\n')

text = (root / 'GoldSrc-SDK/launcher/native/MacRuntimeSession.cpp').read_text()
value, _ = body(text.index('bool Session::Close() noexcept'))
(out / 'launcher_close.inc').write_text(value)

text = (root / 'GoldSrc-SDK/engine/snd_dma.cpp').read_text()
value, _ = body(text.index('void S_ExtraUpdate()'))
(out / 'extra_update.inc').write_text(value)
text = (root / 'GoldSrc-SDK/engine/webm_video.cpp').read_text()
begin = text.index('            bool skipped = false;')
end = text.index('            Sys_Sleep(1);', begin) + len('            Sys_Sleep(1);')
(out / 'webm_dispatch.inc').write_text(text[begin:end])

source_files = ['GoldSrc-SDK/engine/sys_sdlwind.cpp', 'GoldSrc-SDK/engine/sys_dll2.cpp',
                'GoldSrc-SDK/engine/snd_dma.cpp', 'GoldSrc-SDK/engine/webm_video.cpp',
                'GoldSrc-SDK/launcher/native/MacRuntimeSession.cpp']
(out / 'source-pins.json').write_text(json.dumps({relative: hashlib.sha256((root / relative).read_bytes()).hexdigest()
                                               for relative in source_files}, indent=2) + '\n')
(out / 'body-pins.json').write_text(json.dumps({p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                                             for p in out.glob('*.inc')}, indent=2) + '\n')

text = (root / 'GoldSrc-SDK/engine/cdll_int.cpp').read_text()
begin = text.index('static bool nativeClientMouseRequested =')
end = text.index('#endif', begin)
(out / 'mouse_activation_state.inc').write_text(text[begin:end])
for method, name in [('void ClientDLL_SetNativeMouseCapture(bool active)', 'native_mouse_capture'),
                     ('void ClientDLL_ActivateMouse()', 'activate_mouse'),
                     ('void ClientDLL_DeactivateMouse()', 'deactivate_mouse')]:
    value, _ = body(text.index(method))
    (out / (name + '.inc')).write_text(value)

text = (root / 'GoldSrc-SDK/engine/sys_dll2.cpp').read_text()
value, _ = body(text.index('void ClearIOStates()'))
(out / 'clear_io.inc').write_text(value)
