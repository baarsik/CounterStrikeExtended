"""Copy exact owned caller bodies; no branch/tail rewriting or production edits."""
from pathlib import Path
import hashlib
import json
import re
import sys

root = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve()
voice_path = root / 'GoldSrc-SDK/engine/voice.cpp'
voice_bytes = voice_path.read_bytes()
voice = voice_bytes.decode('latin1')
receipts = []


def extract(name):
    matches = list(re.finditer(r'^(?:void|qboolean)\s+' + name + r'\s*\([^;]*?\)\s*\{', voice, re.M))
    assert len(matches) == 1, (name, len(matches))
    begin = matches[0].start()
    index = voice.index('{', matches[0].start())
    depth = 0
    state = 'code'
    while index < len(voice):
        char = voice[index]
        pair = voice[index:index + 2]
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
                index += 1
                break
        index += 1
    assert depth == 0 and index < len(voice)
    body = voice[begin:index]
    receipts.append({'name': name, 'source': str(voice_path), 'source_sha256': hashlib.sha256(voice_bytes).hexdigest(),
        'body_sha256': hashlib.sha256(body.encode('latin1')).hexdigest(), 'line': voice.count('\n', 0, begin) + 1})
    return '#line ' + str(receipts[-1]['line']) + ' "' + voice_path.as_posix() + '"\n' + body + '\n'


helpers = extract('Voice_ReleaseCaptureForModuleUnload') + extract('Voice_ReleaseMixerForModuleUnload')
(out / 'actual_helpers.inc').write_text(helpers)
(out / 'actual_deinit.inc').write_text(extract('Voice_Deinit'))
(out / 'actual_init.inc').write_text(extract('Voice_Init'))
rate = re.search(r'^#define VOICE_SAMPLE_RATE\s+\d+\s*$', voice, re.M).group(0)
header = (root / 'GoldSrc-SDK/engine/voice.h').read_bytes().decode('latin1')
channels = re.search(r'^#define VOICE_NUM_CHANNELS\s+\d+\s*$', header, re.M).group(0)
(out / 'actual_constants.inc').write_text(rate + '\n' + channels + '\n')
begin = voice.index('// The format we sample voice in.')
end = voice.index('#endif', begin) + len('#endif')
format_text = voice[begin:end]
assert 'g_VoiceSampleFormat' in format_text and 'NativeVoiceSampleFormat' in format_text
(out / 'actual_sample_format.inc').write_text('#line ' + str(voice.count('\n', 0, begin) + 1) + ' "'
    + voice_path.as_posix() + '"\n' + format_text + '\n')
receipts.append({'name': 'sample-format', 'body_sha256': hashlib.sha256(format_text.encode('latin1')).hexdigest(),
    'source_sha256': hashlib.sha256(voice_bytes).hexdigest()})
(out / 'actual_bodies.json').write_text(json.dumps(receipts, indent=2) + '\n')
assert voice_path.read_bytes() == voice_bytes, 'caller changed during extraction'
