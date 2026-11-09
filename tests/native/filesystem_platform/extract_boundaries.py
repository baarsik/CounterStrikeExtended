"""Extract exact low-level production methods; adapt only their private class qualifier."""
import argparse
from pathlib import Path
import re

parser = argparse.ArgumentParser()
parser.add_argument('source', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
text = args.source.read_text(encoding='cp1252')
functions = []
for name in ('FS_fseek', 'FS_ftell', 'FS_stat'):
    match = re.search(r'^\w+ CFileSystem_Stdio::' + name + r'\([^;]*?\)\s*\{', text, re.M)
    assert match, name
    depth = 0
    tokens = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\x27(?:\\.|[^\x27\\])*\x27|[{}]', re.S)
    for token in tokens.finditer(text, match.end() - 1):
        if token[0] == '{':
            depth += 1
        elif token[0] == '}':
            depth -= 1
            if depth == 0:
                functions.append(text[match.start():token.end()].replace('CFileSystem_Stdio::', 'LowLevel::', 1))
                break
    else:
        raise ValueError(name)
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text('\n\n'.join(functions) + '\n')
text = (args.source.parent / 'BaseFileSystem.cpp').read_text(encoding='cp1252')
match = re.search(r'^void CBaseFileSystem::FileTimeToString\([^;]*?\)\s*\{', text, re.M)
assert match
depth = 0
for token in tokens.finditer(text, match.end() - 1):
    if token[0] == '{':
        depth += 1
    elif token[0] == '}':
        depth -= 1
        if depth == 0:
            body = text[match.start():token.end()].replace('CBaseFileSystem::', 'Formatter::', 1)
            (args.output.parent / 'actual_formatter.h').write_text(body + '\n')
            break
else:
    raise ValueError('FileTimeToString')
