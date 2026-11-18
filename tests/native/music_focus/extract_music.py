"""Pin complete actual activation/controller bodies; no replacement decision logic."""
from pathlib import Path
import argparse
import hashlib
import json
import re

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
records = []


def span(path, pattern, occurrence=0, optional=False):
    text = path.read_text()
    matches = list(re.finditer(pattern, text, re.M))
    if optional and not matches:
        return ''
    match = matches[occurrence]
    end = text.index('{', match.end()) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    body = text[match.start():end]
    records.append(dict(path=str(path), source_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                        body_sha256=hashlib.sha256(body.encode()).hexdigest(), pattern=pattern,
                        occurrence=occurrence))
    line = text.count(chr(10), 0, match.start()) + 1
    return f'#line {line} "{path}"\n' + body + '\n'


cd = args.source / 'src/engine/cd.cpp'
window = args.source / 'src/engine/sys_sdlwind.cpp'
methods = ['CCDAudio', '~CCDAudio', 'Init', '_Init', 'Pause', 'Resume', '_Pause', '_Resume', '_Stop', 'Stop',
           'Play', 'PlayFile', '_Play', '_PlayFile', 'PrimeTrack', '_PrimeFile', 'Frame', '_CDUpdate',
           '_SwitchToLauncher', '_SwitchToEngine',
           'Shutdown', 'ResetCDTimes', 'MP3_Init', 'MP3_Shutdown', 'MP3_ReleaseDriver', 'MP3_InitStream',
           'MP3_PlayTrack', 'MP3_PlayTrackFinalize', 'MP3_StartStream', 'MP3_StopStream', 'MP3_SetPause',
           'MP3_SetVolume', 'MP3_Suspend_Audio', 'MP3_Resume_Audio']
body = re.search(r'^const float CCDAudio::TRACK_EXTRA_TIME =.*;', cd.read_text(), re.M).group(0) + '\n'
# The optional spans allow a behavioral RED against the frozen original source.
# They never supply replacement logic when the production helper is absent.
body += span(cd, r'^namespace(?= \{\nstruct AppFocusRequests)', optional=True)
body += span(cd, r'^void CDAudio_AppFocusChanged\(', optional=True)
body += span(cd, r'^void CCDAudio::_ApplyAppFocus\(', optional=True)
for name in methods:
    pattern = r'^(?:(?:void|bool|int|float) )?CCDAudio::' + re.escape(name) + r'\('
    count = 2 if name in ['MP3_InitStream', 'MP3_PlayTrack'] else 1
    for occurrence in range(count):
        body += span(cd, pattern, occurrence)
(args.output / 'actual_audio.inc').write_text(body)
for owner, occurrence in [('software', 0), ('hardware', 1)]:
    actual = span(window, r'^void CGame::AppActivate\(', occurrence)
    (args.output / ('actual_activate_' + owner + '.inc')).write_text(actual)
(args.output / 'source-bodies.json').write_text(json.dumps(records, indent=2) + '\n')
