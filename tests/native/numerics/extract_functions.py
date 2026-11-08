"""Compile the selected production bodies without the unrelated engine dependencies.

Inputs are trusted checked-out C++ files. Keep signatures and whole bodies intact;
reject missing/ambiguous definitions rather than silently selecting a declaration.
"""
import argparse
from pathlib import Path
import re


def extract(path, name):
    text = path.read_text(encoding='cp1252')
    matches = list(re.finditer(r'^void ' + re.escape(name) + r'\([^;]*?\)\s*\{', text, re.M))
    if len(matches) != 1:
        raise ValueError(f'{path}: expected exactly one definition of {name}')
    start = matches[0].start()
    depth = 0
    # Skip comments and literals so braces inside them cannot end the function.
    tokens = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]', re.S)
    for token in tokens.finditer(text, matches[0].end() - 1):
        if token[0] == '{':
            depth += 1
        elif token[0] == '}':
            depth -= 1
            if depth == 0:
                return text[start:token.end()]
    raise ValueError(f'{path}: unterminated function {name}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    bodies = [extract(args.source / 'common.cpp', 'MSG_WriteBitAngle'),
              extract(args.source / 'cvar.cpp', 'Cvar_SetValue'),
              extract(args.source / 'cvar.cpp', 'Cmd_CvarListPrintCvar')]
    (args.output / 'actual_angle.h').write_text(bodies[0] + '\n')
    (args.output / 'actual_cvar.h').write_text('\n\n'.join(bodies[1:]) + '\n')
