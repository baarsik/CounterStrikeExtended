"""Compile complete production Draw/layout bodies; replace only external rendering dependencies."""
import hashlib
import json
from pathlib import Path
import sys


def block(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


root = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2])
out.mkdir(parents=True, exist_ok=True)
paths = ['src/cstrike/cl_dll/death.cpp', 'src/cstrike/cl_dll/cl_util.h']
death, util = [(root / path).read_text(encoding='latin1') for path in paths]
notices = [next(line for line in death.splitlines() if line.startswith('#define MAX_DEATHNOTICES')),
           block(death, 'struct DeathNoticeItem') + ';']
for prefix in ['static int DEATHNOTICE_DISPLAY_TIME', 'DeathNoticeItem rgDeathNoticeList']:
    notices.append(next(line for line in death.splitlines() if line.startswith(prefix)))
(out / 'notices.inc').write_text('\n'.join(notices) + '\n')
bodies = [block(util, signature) for signature in
          ['inline int DrawConsoleString(', 'inline void GetConsoleStringSize(', 'inline int ConsoleStringLen(']]
bodies += [block(death, signature) for signature in
           ['int CHudDeathNotice::GetDeathNoticeY()', 'int CHudDeathNotice::Draw(']]
(out / 'bodies.inc').write_text('\n\n'.join(bodies) + '\n')
(out / 'source-manifest.json').write_text(json.dumps({
    path: hashlib.sha256((root / path).read_bytes()).hexdigest() for path in paths
}, indent=2) + '\n')
