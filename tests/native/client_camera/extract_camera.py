"""Extract unchanged camera state and complete ownership entry bodies."""
from pathlib import Path
import hashlib
import json
import re
import sys

root = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve()
out.mkdir(parents=True, exist_ok=True)
records = []

for game, relative in [('hl', 'cl_dll'), ('cs', 'cstrike/cl_dll')]:
    path = root / 'src' / relative / 'in_camera.cpp'
    data = path.read_bytes()
    text = data.decode('latin1')
    start = re.search(r'^cvar_t\s*\*\s*cam_command;', text, re.M).start()
    stop = text.index('void SDL_GetCursorPos', start)
    state = text[start:stop]
    # The native helper is included in this state range after integration.
    constants = text[text.index('#define CAM_DIST_DELTA'):text.index('//-------------------------------------------------- Global Variables')] \
        if game == 'hl' else ''
    blocks = [constants, state]
    for name in ['SDL_GetCursorPos', 'SDL_SetCursorPos', 'MoveToward', 'CAM_ToThirdPerson',
                 'CAM_ToFirstPerson', 'CAM_Think', 'CAM_ClearStates', 'CAM_StartMouseMove', 'CAM_EndMouseMove',
                 'CAM_StartDistance', 'CAM_EndDistance']:
        pattern = r'^(?:extern "C" )?(?:void|float)\s+(?:(?:CL_DLLEXPORT|EXPORT)\s+)?' \
            + name + r'\s*\([^;]*?\)\s*\{'
        match = re.search(pattern, text, re.M)
        assert match, name
        begin = match.start()
        end = text.index('{', begin) + 1
        depth = 1
        while depth:
            depth += (text[end] == '{') - (text[end] == '}')
            end += 1
        body = text[begin:end]
        blocks.append(body)
        records.append({'game': game, 'name': name, 'source_sha256': hashlib.sha256(data).hexdigest(),
                        'body_sha256': hashlib.sha256(body.encode('latin1')).hexdigest()})
    (out / (game + '_camera.inc')).write_text('\n'.join(blocks) + '\n')
(out / 'extracted.json').write_text(json.dumps(records, indent=2) + '\n')
