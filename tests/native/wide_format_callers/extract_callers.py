"""Extract exact owned caller branches; dependencies are bound explicitly by the fixture."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

parser = argparse.ArgumentParser()
parser.add_argument('--root', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
root = args.root
inputs = {}
def read(name):
    path = root / name
    raw = path.read_bytes()
    inputs[name] = hashlib.sha256(raw).hexdigest()
    return raw.decode('cp1252')
hud = read('GoldSrc-SDK/cstrike/cl_dll/hud_vgui2print.cpp')
start = hud.index('#ifdef _WIN32\n\twchar_t format[1024];')
end = hud.index('\n}\n\nint CHudVGUI2Print::GetHudFontHeight', start)
(args.output / 'actual_hud.inc').write_text(hud[start:end] + '\n')
chat = read('GoldSrc-SDK/cstrike/cl_dll/saytext.cpp')
start = chat.index('\twchar_t* format = wcsstr( localized, L"%s" );')
end = chat.index('\n\tg_sayTextLine[line].SetText(', start)
(args.output / 'actual_chat.inc').write_text(chat[start:end] + '\n')
# Wrapper extraction already records its complete production input and adaptation.
extractor = Path(__file__).resolve().parent.parent / 'wide_format/extract_wrappers.py'
subprocess.run([sys.executable, str(extractor), str(root), str(args.output / 'actual_wrappers.h')], check=True)
voice = read('GoldSrc-SDK/cstrike/game_shared/voice_status_hud.cpp')
start = voice.index('void CVoiceLabel::RebuildLabelText()')
end = voice.index('\n#ifdef _MSC_VER', start)
(args.output / 'actual_voice.inc').write_text(voice[start:end] + '\n')
for name, variable, output in [('GameUI/labelslidercombo.cpp', 'text', 'actual_numeric64.inc'),
                               ('GameUI/CareerGameTasks.cpp', 'repeat', 'actual_numeric16.inc')]:
    text = read('GoldSrc-SDK/' + name)
    call = text.index('swprintf( ' + variable)
    line = text.rfind('\n', 0, call) + 1
    start = text.rfind('#if defined(_WIN32)', 0, line)
    assert start >= 0
    end = text.index('#endif', call) + len('#endif')
    (args.output / output).write_text(text[start:end] + '\n')
(args.output / 'inputs.json').write_text(json.dumps(inputs, indent=2) + '\n')
