"""Import unchanged pinned SDK bodies, with macro-only counting wrappers."""
from pathlib import Path
import hashlib
import json
import re
import sys

source = Path(sys.argv[1]).resolve()
output = Path(sys.argv[2]).resolve()
output.mkdir(parents=True, exist_ok=True)
expected = {
    "src/SDL.c": "bc7580f2b10df65fbe76760af1b4127c96a3fc1ba971613d65490c21e10bed09",
    "src/SDL_assert.c": "8088036bb52238b28a94bb6e86d82eddf945e76063a4a540f191c1660238d496",
    "src/video/SDL_video.c": "f6d7f1f4423ae9d9bb68169d4e1184fed2f1ebb746d14f896c99e8c64aa2d9d1",
    "src/events/SDL_mouse.c": "29b8bc37daf766170aeef707116d752aa0edcd2bc9c3f687e66389c74c333f25",
    "src/events/SDL_mouse_c.h": "9fa02f5ad1c4e33087e26b0556bfcb2b8f6a1c4e0ff3377890f78b19a7d6cdfc",
    "src/video/SDL_sysvideo.h": "e70ee9d6821c173313759f44cf40635f598b8aed6352edb50a987f7a5c095a6f",
}
sha = lambda data: hashlib.sha256(data).hexdigest()
for relative, value in expected.items():
    assert sha((source / relative).read_bytes()) == value, relative

records = []
definitions = [
    ("src/SDL.c", "SDL_IsMainThread", "bool", "void", "", "FixtureMainCheck", "FixtureActualMain"),
    ("src/SDL.c", "SDL_WasInit", "Uint32", "SDL_InitFlags flags", "flags", "FixtureInitCheck", "FixtureActualInit"),
    ("src/video/SDL_video.c", "SDL_GetVideoDevice", "SDL_VideoDevice *", "void", "", "FixtureDeviceCheck", "FixtureActualDevice"),
    ("src/video/SDL_video.c", "SDL_OnVideoThread", "bool", "void", "", "FixtureVideoCheck", "FixtureActualVideo"),
    ("src/events/SDL_mouse.c", "SDL_GetMouse", "SDL_Mouse *", "void", "", "FixtureMouseCheck", "FixtureActualMouse"),
    ("src/events/SDL_mouse.c", "SDL_GetRelativeMouseMode", "bool", "void", "", "FixtureModeCheck", "FixtureActualMode"),
    ("src/SDL_assert.c", "SDL_GetDefaultAssertionHandler", "SDL_AssertionHandler", "void", "", "FixtureDefaultCheck", "FixtureActualDefault"),
    ("src/SDL_assert.c", "SDL_GetAssertionHandler", "SDL_AssertionHandler", "void **userdata", "userdata", "FixtureCurrentHandlerCheck", "FixtureActualCurrent"),
    ("src/SDL_assert.c", "SDL_SetAssertionHandler", "void", "SDL_AssertionHandler handler, void *userdata", "handler, userdata", "FixtureSetterCheck", "FixtureActualSetter"),
]
chunks = [re.search(r"^#define SDL_ALL_SUBSYSTEM_FLAGS[^\n]*", (source / "src/SDL.c").read_text(), re.M)[0]]
for relative, name, result, parameters, argument, counter, actual in definitions:
    path = source / relative
    data = path.read_bytes()
    text = data.decode()
    match = re.search(r"^(?:void|bool|Uint32|SDL_VideoDevice\s*\*|SDL_Mouse\s*\*|SDL_AssertionHandler)\s*" + name + r"\s*\([^;]*?\)\s*\{", text, re.M)
    assert match, name
    end = text.index("{", match.start()) + 1
    depth = 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    body = text[match.start():end]
    mapped = name + "_REAL" if name in ["SDL_IsMainThread", "SDL_WasInit", "SDL_GetDefaultAssertionHandler", "SDL_GetAssertionHandler", "SDL_SetAssertionHandler"] else name
    prefix = "" if result == "void" else "return "
    chunks += [f"#define {mapped} {actual}", body, f"#undef {mapped}",
               f"{result} {mapped}({parameters}) {{\n    ++fixtureCounts[{counter}];\n    {prefix}{actual}({argument});\n}}"]
    records.append(dict(relative=relative, name=name, source_sha256=sha(data), body_sha256=sha(body.encode()),
                        adaptation="Unchanged body text; macro-only name redirection and external counting wrapper"))
license_text = (source / "src/events/SDL_mouse.c").read_text().split("#include", 1)[0]
(output / "sdk_bodies.inc").write_text(license_text + "\n\n".join(chunks) + "\n")
(output / "imported-sdk.json").write_text(json.dumps(records, indent=2) + "\n")
