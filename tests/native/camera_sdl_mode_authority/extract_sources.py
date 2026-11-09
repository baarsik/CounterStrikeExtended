"""Import pinned SDL bodies unchanged; declarations come from complete vendor headers."""
from pathlib import Path
import hashlib
import json
import re
import sys

sdl = Path(sys.argv[1]).resolve()
repo = Path(sys.argv[2]).resolve()
out = Path(sys.argv[3]).resolve()
out.mkdir(parents=True, exist_ok=True)
sha = lambda b: hashlib.sha256(b).hexdigest()
mouse = sdl / 'src/events/SDL_mouse.c'
video = sdl / 'src/video/SDL_video.c'
expected = {'src/events/SDL_mouse.c':'29b8bc37daf766170aeef707116d752aa0edcd2bc9c3f687e66389c74c333f25',
 'src/events/SDL_mouse_c.h':'9fa02f5ad1c4e33087e26b0556bfcb2b8f6a1c4e0ff3377890f78b19a7d6cdfc',
 'src/video/SDL_video.c':'f6d7f1f4423ae9d9bb68169d4e1184fed2f1ebb746d14f896c99e8c64aa2d9d1'}
for relative, value in expected.items():
    assert sha((sdl / relative).read_bytes()) == value, relative
records = []

def function(path, name):
    data = path.read_bytes()
    text = data.decode('utf-8')
    match = re.search(r'^(?:(?:static|inline)\s+)*(?:bool|void|SDL_Mouse\s*\*|SDL_Window\s*\*|SDL_MouseButtonFlags|const SDL_Rect\s*\*)\s*(?:SDLCALL\s+)?' + name + r'\s*\([^;]*?\)\s*\{', text, re.M)
    assert match, (path, name)
    end = text.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    body = text[match.start():end]
    records.append(dict(path=str(path), name=name, source_sha256=sha(data), body_sha256=sha(body.encode())))
    return body

m = mouse.read_text()
v = video.read_text()
license = m[:m.index('#include')]
macro_at = v.index('#define CHECK_WINDOW_MAGIC(')
macro_end = v.index('\n\n', macro_at)
popup_at = v.index('#define CHECK_WINDOW_NOT_POPUP(')
popup_end = v.index('\n\n', popup_at)
blocks = [license, '/* Fixture import: unchanged bodies; typed fake platform seams supplied by sdk_fixture.c. */',
    v[macro_at:macro_end], v[popup_at:popup_end], '#define WARP_EMULATION_THRESHOLD_NS SDL_MS_TO_NS(30)',
    'static SDL_Mouse SDL_mouse;', 'static bool SDL_mouse_initialized;', 'static SDL_HashTable *SDL_mouse_names;', 'static bool track_mouse_down = false;',
    'static void SDL_PrivateSendMouseMotion(Uint64, SDL_Window *, SDL_MouseID, bool, float, float);']
for name in ['SDL_GetMouse','SDL_GetMouseFocus','SDL_GetMouseButtonState','SDL_MousePositionInWindow',
             'SDL_UpdateMouseFocus','SDL_SendMouseMotion','ConstrainMousePosition','SDL_PrivateSendMouseMotion',
             'SDL_GetMouseState','SDL_GetRelativeMouseState','SDL_PerformWarpMouseInWindow','SDL_DisableMouseWarpEmulation',
             'SDL_MaybeEnableWarpEmulation','SDL_WarpMouseInWindow','SDL_SetRelativeMouseMode',
             'SDL_GetRelativeMouseMode','SDL_UpdateRelativeMouseMode','SDL_ShowCursor','SDL_HideCursor',
             'SDL_MouseWarpEmulationChanged','SDL_MouseDoubleClickTimeChanged','SDL_MouseDoubleClickRadiusChanged',
             'SDL_MouseNormalSpeedScaleChanged','SDL_MouseRelativeSpeedScaleChanged','SDL_MouseRelativeModeCenterChanged',
             'SDL_MouseRelativeSystemScaleChanged','SDL_TouchMouseEventsChanged','SDL_MouseTouchEventsChanged',
             'SDL_PenMouseEventsChanged','SDL_MouseAutoCaptureChanged','SDL_MouseRelativeWarpMotionChanged',
             'SDL_MouseRelativeCursorVisibleChanged','SDL_MouseIntegerModeChanged','SDL_PreInitMouse']:
    blocks.append(function(mouse,name))
for name in ['SDL_SetWindowRelativeMouseMode','SDL_GetWindowRelativeMouseMode','SDL_GetWindowMouseRect','SDL_SetWindowMouseGrab','SDL_GetWindowSize']:
    blocks.append(function(video,name))
(out/'sdk_bodies.inc').write_text('\n\n'.join(blocks)+'\n')
# Camera extractor imports complete current bodies with existing scaffolding; no source body replacement.
import subprocess
subprocess.run([sys.executable,str(repo/'tests/native/client_camera/extract_camera.py'),str(repo),str(out)],check=True)
(out/'imported-sdl.json').write_text(json.dumps(records,indent=2)+'\n')
closure=[dict(path=str(sdl/p),sha256=sha((sdl/p).read_bytes())) for p in ['src/events/SDL_mouse_c.h','src/video/SDL_sysvideo.h','include/build_config/SDL_build_config_minimal.h']]
(out/'declaration-pins.json').write_text(json.dumps(closure,indent=2)+'\n')

scaffold_path = repo / 'tests/native/client_camera/ownership_tests.cpp'
scaffold_original = scaffold_path.read_text()
scaffold = scaffold_original.replace('std::abort();', 'std::exit(17);')
old = '    *x = services.x;\n    *y = services.y;'
new = '    if (fixtureCameraUsesSdk) {\n        float positionX = 0.0f;\n        float positionY = 0.0f;\n        FixtureMousePosition(&positionX, &positionY);\n        *x = static_cast<int>(positionX);\n        *y = static_cast<int>(positionY);\n        *centerX = FixtureWindow(0)->w / 2;\n        *centerY = FixtureWindow(0)->h / 2;\n        return 1;\n    }\n    *x = services.x;\n    *y = services.y;'
assert scaffold.count(old) == 1
scaffold = scaffold.replace(old, new)
old = '        services.warps.emplace_back(x, y);'
new = old + '\n        if (fixtureCameraUsesSdk) {\n            FixtureWarp(static_cast<float>(x), static_cast<float>(y));\n        }'
assert scaffold.count(old) == 1
scaffold = scaffold.replace(old, new)
(out/'camera_scaffold.inc').write_text(scaffold)
(out/'scaffold-adaptation.json').write_text(json.dumps(dict(source=str(scaffold_path),
    source_sha256=sha(scaffold_original.encode()), generated_sha256=sha(scaffold.encode()),
    changes=['Typed SDK-backed query seam when enabled', 'Typed SDK warp callback when enabled', 'Test assertion exits17 instead of abort/crash reporting'],
    production_camera_bodies_changed=False, abi_claim=False), indent=2)+'\n')

callers=[]
for name,relative in [('base','GoldSrc-SDK/engine/vgui2/BaseUISurface.cpp'),('wrap','GoldSrc-SDK/engine/VGUI_EngineSurfaceWrap.cpp')]:
    path=repo/relative
    data=path.read_bytes()
    text=data.decode('latin1')
    begin=text.index('    const bool useRawInput =')
    marker='    s_bUsedRawInputLast = useRawInput;'
    end=text.index(marker,begin)+len(marker)
    block=text[begin:end]
    (out/(name+'_caller.inc')).write_text(block+'\n')
    callers.append(dict(name=name,path=str(path),source_sha256=sha(data),block_sha256=sha(block.encode('latin1'))))
(out/'caller-blocks.json').write_text(json.dumps(callers,indent=2)+'\n')

mutant = sys.argv[4] if len(sys.argv) > 4 else 'none'
changes = {
 'no-prohibition':('sdk_bodies.inc','    mouse->warp_emulation_prohibited = true;','    /* Deliberate mutant: no prohibition. */'),
 'failed-global-commit':('sdk_bodies.inc','    if (!mouse->SetRelativeMouseMode(enabled)) {\n        return false;\n    }','    mouse->SetRelativeMouseMode(enabled);'),
 'relative-cache-stale':('sdk_bodies.inc','            mouse->x = x;\n            mouse->y = y;\n            mouse->has_position = true;','            mouse->has_position = true;'),
 'warp-consumes-accumulator':('sdk_bodies.inc','            mouse->x = x;\n            mouse->y = y;\n            mouse->has_position = true;','            mouse->x = x;\n            mouse->y = y;\n            mouse->has_position = true;\n            mouse->x_accu = 0;\n            mouse->y_accu = 0;'),
 'nochange-updates-global':('sdk_bodies.inc','    if (enabled == SDL_GetWindowRelativeMouseMode(window)) {\n        return true;\n    }','    if (enabled == SDL_GetWindowRelativeMouseMode(window)) {\n        return SDL_UpdateRelativeMouseMode();\n    }'),
 'no-flag-rollback':('sdk_bodies.inc','        if (enabled) {\n            window->flags &= ~SDL_WINDOW_MOUSE_RELATIVE_MODE;\n        } else {\n            window->flags |= SDL_WINDOW_MOUSE_RELATIVE_MODE;\n        }','        /* Deliberate mutant: keep failed requested flag. */'),
 'caller-failure-commit':('base_caller.inc','    s_bUsedRawInputLast = useRawInput;','    s_bUsedRawInputLast = !useRawInput;')
}
if mutant != 'none':
    name, old, new = changes[mutant]
    path=out/name
    original=path.read_text()
    assert original.count(old)==1,(mutant,original.count(old))
    changed=original.replace(old,new)
    path.write_text(changed)
    (out/'mutation.json').write_text(json.dumps(dict(name=mutant,file=name,
        before=old,after=new,original_generated_sha256=sha(original.encode()),
        changed_generated_sha256=sha(changed.encode()),production_source_changed=False),indent=2)+'\n')
