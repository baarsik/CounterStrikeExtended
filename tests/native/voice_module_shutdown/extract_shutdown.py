from pathlib import Path
import re
import sys

root = Path(sys.argv[1])
out = Path(sys.argv[2])


def body(text, expression):
    match = re.search(expression, text)
    if not match:
        return None
    start = text.index("{", match.end())
    end = start + 1
    depth = 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[match.start():end]


voice = (root / "GoldSrc-SDK/engine/voice.cpp").read_bytes().decode("cp1252")
release = body(voice, r"void Voice_ReleaseCaptureForModuleUnload\(\)")
if release is None:
    # Executable pre-change reference: the exact old normal-deinit release arm.
    deinit = voice[voice.index("void Voice_Deinit()") :]
    block = body(deinit, r"if\s*\(\s*g_pVoiceRecord\s*\)")
    assert block is not None
    release = "void Voice_ReleaseCaptureForModuleUnload() {\n" + block + "\n}"
mixer = body(voice, r"void Voice_ReleaseMixerForModuleUnload\(\)")
if mixer is None:
    deinit = voice[voice.index("void Voice_Deinit()") :]
    block = body(deinit, r"if\s*\(\s*g_pMixerControls\s*\)")
    assert block is not None
    mixer = "void Voice_ReleaseMixerForModuleUnload() {\n" + block + "\n}"
cd = (root / "GoldSrc-SDK/engine/cd.cpp").read_bytes().decode("cp1252")
shutdown = body(cd, r"bool CDAudio_ShutdownForModuleUnload\(\)")
assert shutdown is not None
(out / "actual_capture_close.inc").write_text(release, encoding="utf-8")
(out / "actual_mixer_close.inc").write_text(mixer, encoding="utf-8")
(out / "actual_audio_close.inc").write_text(shutdown, encoding="utf-8")
