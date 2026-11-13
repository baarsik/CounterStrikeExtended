from pathlib import Path
import hashlib
import json
import sys

repo = Path(sys.argv[1]).resolve()
output = Path(sys.argv[2]).resolve()
output.mkdir(parents=True, exist_ok=True)
rows = []
for name, relative, method in [
    ('base', 'src/engine/vgui2/BaseUISurface.cpp', 'void BaseUISurface::SetCursor('),
    ('wrap', 'src/engine/VGUI_EngineSurfaceWrap.cpp', 'void EngineSurfaceWrap::setCursor('),
]:
    data = (repo / relative).read_bytes()
    text = data.decode('latin1')
    begin = text.index(method)
    opening = text.index('{', begin)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    full = text[begin:end]
    (output / (name + '_full.inc')).write_bytes(full.encode('latin1'))
    block = full[full.index('    const bool useRawInput ='): -1]
    (output / (name + '_caller.inc')).write_bytes(block.encode('latin1'))
    rows.append(dict(relative=relative, method=method, source_sha256=hashlib.sha256(data).hexdigest(),
        full_method_sha256=hashlib.sha256(full.encode('latin1')).hexdigest(),
        tail_sha256=hashlib.sha256(block.encode('latin1')).hexdigest(),
        adaptation='Exact complete post-cursor-selection tail; typed game/cvar services supplied by fixture'))
(output / 'caller-source-pins.json').write_text(json.dumps(rows, indent=2) + '\n')

source = repo / 'src/engine/sys_sdlwind.cpp'
data = source.read_bytes()
text = data.decode('latin1')
startups = []
shutdowns = []
for method, target in [('bool CGame::CreateGameWindow()', startups), ('bool CGame::Shutdown()', shutdowns)]:
    offset = 0
    while True:
        begin = text.find(method, offset)
        if begin < 0:
            break
        opening = text.index('{', begin)
        depth = 1
        end = opening + 1
        while depth:
            depth += (text[end] == '{') - (text[end] == '}')
            end += 1
        target.append(text[opening + 1:end - 1])
        offset = end
assert len(startups) == len(shutdowns) == 2
startup_blocks = []
for body in startups:
    begin = body.index('#if INTPTR_MAX > INT32_MAX')
    end = body.index('#endif', begin) + len('#endif')
    block = body[begin:end]
    assert 'NativeMouseWindowState().Bind(m_hSDLWindow)' in block
    assert begin > body.rfind('SDL_CreateWindow(')
    assert end < body.index('SDL_ShowWindow(')
    startup_blocks.append(block)
assert startup_blocks[0] == startup_blocks[1]
shutdown_blocks = [body[body.index('#if defined(__APPLE__)'):body.index('    if (m_hSDLRenderer && m_bCreatedRenderer)')]
                   for body in shutdowns]
assert shutdown_blocks[0] == shutdown_blocks[1]
assert shutdown_blocks[0].index('RendererWillDestroy') < shutdown_blocks[0].index('NativeMouseWindowState().Invalidate')
(output / 'hidden_bind.inc').write_bytes(startup_blocks[0].encode('latin1'))
(output / 'shutdown_prefix.inc').write_bytes(shutdown_blocks[0].encode('latin1'))
(output / 'lifecycle-source-pins.json').write_text(json.dumps(dict(relative=str(source.relative_to(repo)),
    source_sha256=hashlib.sha256(data).hexdigest(), software_and_hardware_blocks_identical=True,
    bind_after_last_create_before_show=True, font_retirement_before_invalidation=True,
    bind_sha256=hashlib.sha256(startup_blocks[0].encode('latin1')).hexdigest(),
    shutdown_prefix_sha256=hashlib.sha256(shutdown_blocks[0].encode('latin1')).hexdigest(),
    scope='Exact hook blocks and shutdown retirement section; preceding software ReleaseVideo and renderer lifecycle are outside fixture'), indent=2) + '\n')

text = (repo / 'src/engine/vgui2/BaseUISurface.cpp').read_text(encoding='latin1')
begin = text.index('    SetCursor(vgui2::dc_none);', text.index('void BaseUISurface::CalculateMouseVisible()'))
end = text.index('\n}\n', begin)
(output / 'base_no_popup.inc').write_text(text[begin:end])
