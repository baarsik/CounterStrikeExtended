"""Windows source-conditional invariant, independent of unowned SDK/header drift.

Only actual Apple conditional groups are selected here. Other directives remain
verbatim, so every retained non-Apple branch is compared. Clang's raw lexer then
compares exact token kind/spelling, ignoring whitespace/comments/locations. This
is NOT an MSVC preprocessor/compiler invocation or new Windows runtime evidence.
"""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--before', required=True, type=Path)
parser.add_argument('--after', required=True, type=Path)
parser.add_argument('--output', required=True, type=Path)
args = parser.parse_args()
args.output.mkdir(exist_ok=False)


def select_nonapple(text):
    stack = []
    enabled = True
    result = []
    for line in text.splitlines(keepends=True):
        directive = re.match(r'\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)', line)
        if not directive:
            if enabled:
                result.append(line)
            continue
        name, expression = directive.groups()
        if name in ['if', 'ifdef', 'ifndef']:
            normalized = re.sub(r'\s+', '', expression)
            known = name == 'if' and normalized in ['defined(__APPLE__)', '!defined(__APPLE__)']
            condition = normalized == '!defined(__APPLE__)'
            stack.append((known, enabled, condition))
            if known:
                enabled = enabled and condition
            elif enabled:
                result.append(line)
        elif name == 'else':
            known, parent, condition = stack[-1]
            if known:
                enabled = parent and not condition
            elif enabled:
                result.append(line)
        elif name == 'elif':
            assert not stack[-1][0], 'unknown Apple elif structure'
            if enabled:
                result.append(line)
        else:
            known, parent, _ = stack.pop()
            if not known and enabled:
                result.append(line)
            enabled = parent
    assert not stack
    return ''.join(result)


def tokens(path, log):
    command = ['xcrun', 'clang++', '-std=c++14', '-fsyntax-only', '-Xclang', '-dump-raw-tokens', str(path)]
    result = subprocess.run(command, capture_output=True, text=True, timeout=45)
    log.write_text(result.stderr)
    assert result.returncode == 0
    values = []
    for line in result.stderr.splitlines():
        found = re.match(r"^(\w+) '(.*)'(?:\s+\[.*\])?\s+Loc=<", line)
        if found and found[1] not in ['unknown', 'comment']:
            values.append([found[1], found[2]])
    assert values
    return values, command


names = ['VGUI_EngineSurfaceWrap.cpp', 'vgui_intwrap.cpp', 'vgui_EngineSurface.cpp',
         'vgui_EngineSurfaceHW.cpp', 'vgui_EngineSurfaceSW.cpp', 'vgui_EngineSurface.h', 'sys_sdlwind.cpp']
rows = []
for name in names:
    before = args.before / 'engine' / name
    if not before.exists() and name == 'vgui_EngineSurface.cpp':
        before = args.before / 'engine/VGUI_EngineSurface.cpp'
    after = args.after / 'GoldSrc-SDK/engine' / name
    row = {'source': name, 'before': hashlib.sha256(before.read_bytes()).hexdigest(),
           'after': hashlib.sha256(after.read_bytes()).hexdigest()}
    streams = []
    for phase, path in [('before', before), ('after', after)]:
        selected = args.output / (name + '-' + phase + '.cpp')
        selected.write_text(select_nonapple(path.read_text(encoding='cp1252')))
        stream, command = tokens(selected, args.output / (name + '-' + phase + '.raw-tokens.log'))
        streams.append(stream)
        row[phase + '_command'] = command
    row['equal'] = streams[0] == streams[1]
    row['token_count'] = len(streams[0])
    (args.output / (name + '-tokens.json')).write_text(json.dumps(streams[1], indent=2) + '\n')
    rows.append(row)
    assert row['equal'], name
(args.output / 'results.json').write_text(json.dumps({'scope': __doc__, 'files': rows}, indent=2) + '\n')
print('PASS all7 selected non-Apple source token streams unchanged; no Windows compile/runtime claim.')
