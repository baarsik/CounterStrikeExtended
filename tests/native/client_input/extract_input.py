"""Extract complete retained mouse functions and state without rewriting their bodies."""
from pathlib import Path
import hashlib
import json
import re
import sys

root = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)
receipts = []
functions = ["Force_CenterView_f", "IN_SetCameraMouseOwnership", "IN_ActivateMouse", "IN_DeactivateMouse", "IN_StartupMouse", "IN_Shutdown",
             "IN_GetMousePos", "IN_ResetMouse", "IN_MouseEvent", "IN_ScaleMouse", "IN_MouseMove", "IN_Accumulate",
             "IN_ClearStates", "IN_Move", "IN_Init"]


def complete_function(text, name):
    pattern = r'^(?:(?:extern "C" |static )?(?:void|float|long|bool)\s+(?:CL_DLLEXPORT\s+)?)' + name + r'\s*\([^;]*?\)\s*\{'
    matches = list(re.finditer(pattern, text, re.M))
    assert len(matches) == 1, (name, len(matches))
    return complete_block(text, matches[0].start())


def complete_block(text, begin):
    index = text.index('{', begin)
    depth = 0
    state = 'code'
    while index < len(text):
        char = text[index]
        pair = text[index:index + 2]
        if state == 'line':
            if char == '\n':
                state = 'code'
        elif state == 'block':
            if pair == '*/':
                state = 'code'
                index += 1
        elif state in ['"', "'"]:
            if char == '\\':
                index += 1
            elif char == state:
                state = 'code'
        elif pair == '//':
            state = 'line'
            index += 1
        elif pair == '/*':
            state = 'block'
            index += 1
        elif char in ['"', "'"]:
            state = char
        elif char == '{':
            depth += 1
        elif char == '}':
            depth -= 1
            if depth == 0:
                return text[begin:index + 1], begin
        index += 1
    raise AssertionError(f'unterminated block at source offset {begin}')


for game, relative in [('hl', 'src/cl_dll/inputw32.cpp'),
                       ('cs', 'src/cstrike/cl_dll/inputw32.cpp')]:
    path = root / relative
    data = path.read_bytes()
    text = data.decode('latin1')
    def record(name, body, begin):
        receipts.append({'game': game, 'name': name, 'source': relative, 'source_sha256': hashlib.sha256(data).hexdigest(),
                         'body_sha256': hashlib.sha256(body.encode('latin1')).hexdigest(),
                         'line': text.count('\n', 0, begin) + 1})
        return '#line ' + str(receipts[-1]['line']) + ' "' + relative + '"\n' + body + '\n'
    begin = text.index('static bool m_bMouseThread' if game == 'hl' else 'static double s_flRawInputUpdateTime')
    # After the repair CS raw state intentionally precedes the Windows-only thread flag.
    if game == 'cs':
        begin = text.index('static double s_flRawInputUpdateTime')
        if text[begin - len('#ifdef _WIN32\n'):begin] == '#ifdef _WIN32\n':
            begin -= len('#ifdef _WIN32\n')
    end = text.index('// joystick defines', begin)
    globals_text = 'static cse::FractionalMotion s_relativeMotion;\nint g_iVisibleMouse = 0;\n'
    globals_text += record('mouse-state', text[begin:end], begin)
    begin = text.index('cvar_t', text.index('// none of these cvars'))
    end = text.index('/*', text.index('joy_haspov;', begin))
    globals_text += record('joystick-state-and-thread-handles', text[begin:end], begin)
    globals_text += 'SDL_Gamepad *s_pJoystick = nullptr;\n'
    (out / (game + '_state.inc')).write_text(globals_text)
    bodies = []
    order = list(functions)
    if game == 'hl':
        order.insert(order.index('IN_ResetMouse'), 'IN_GetMouseSensitivity')
        order.insert(order.index('IN_MouseEvent'), 'IN_ResetRelativeMouseState')
        # IN_UseRawInput is included verbatim in mouse-state above.
    native_helpers = ['IN_MousePollingAllowed', 'IN_StopMousePolling', 'IN_DisableMousePolling',
                      'IN_PublishMousePolling', 'IN_ConsumeMousePolling']
    bodies.append('#if defined(_WIN32) && defined(_WIN64)\n')
    native_helpers.insert(0, 'IN_ReadDirectMousePosition')
    if game == 'cs':
        native_helpers.insert(0, 'IN_RefreshRawInputCache')
    for name in native_helpers:
        body, begin = complete_function(text, name)
        bodies.append(record(name, body, begin))
    bodies.append('#endif\n')
    for name in order:
        body, begin = complete_function(text, name)
        if game == 'cs' and name == 'IN_MouseMove':
            context = text.rindex('#pragma warning( push )', 0, begin)
            bodies.append(record('mouse-warning-push', text[context:begin], context))
        if name == 'IN_SetCameraMouseOwnership':
            assert text[:begin].rstrip().endswith('#if UINTPTR_MAX > UINT32_MAX')
            bodies.append('#if UINTPTR_MAX > UINT32_MAX\n')
        bodies.append(record(name, body, begin))
        if name == 'IN_SetCameraMouseOwnership':
            assert text[begin + len(body):].lstrip().startswith('#endif')
            bodies.append('#endif\n')
        if game == 'cs' and name == 'IN_MouseMove':
            context = text.index('#pragma warning( pop )', begin + len(body))
            bodies.append(record('mouse-warning-pop', text[context:context + len('#pragma warning( pop )')], context))
    (out / (game + '_functions.inc')).write_text('\n'.join(bodies))
    assert path.read_bytes() == data, 'production changed during extraction'
(out / 'actual_bodies.json').write_text(json.dumps(receipts, indent=2) + '\n')

# Execute the complete retained event-loop mouse branch with real game consumers.
engine = root / 'src/engine/sys_sdlwind.cpp'
data = engine.read_bytes()
text = data.decode('latin1')
blocks = []
offset = 0
needle = '} else if (ev.type == SDL_EVENT_MOUSE_MOTION'
while (begin := text.find(needle, offset)) >= 0:
    begin += len('} else ')
    block, _ = complete_block(text, begin)
    blocks.append(block)
    offset = begin + len(block)
assert len(blocks) == 2 and blocks[0] == blocks[1]
(out / 'engine_motion.inc').write_text(blocks[0])
(out / 'engine_motion.json').write_text(json.dumps(dict(source=str(engine.relative_to(root)),
    source_sha256=hashlib.sha256(data).hexdigest(), body_sha256=hashlib.sha256(blocks[0].encode('latin1')).hexdigest(),
    scope='Exact identical HW/SW mouse-motion branch; event queue and OS warp/pump are controlled seams'), indent=2) + '\n')
