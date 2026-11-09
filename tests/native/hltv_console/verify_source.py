#!/usr/bin/env python3
"""Pin Windows-selected tokens and isolate the explicitly approved shared history exception."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[3]
CONSOLE = ROOT / 'GoldSrc-SDK/HLTV/Console'
TOKEN = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[A-Za-z_]\w*|[^\s]')


def selected(data, target):
    text = re.sub(r'^\s*#\s*include[^\n]*', '', data.decode(), flags=re.M)
    command = ['/usr/bin/clang++', '-E', '-P', '-x', 'c++', '--target=' + target, '-D_WIN32',
               '-D_MSC_VER=1951', '-DHLTV_CONSOLE', '-Wno-pragma-once-outside-header', '-']
    result = subprocess.run(command, input=text.encode(), capture_output=True, check=True)
    return TOKEN.findall(result.stdout.decode())


def methods(data):
    text = data.decode()
    found = {}
    for match in re.finditer(r'(?:\w+\s+)?CTextConsole::(~?\w+)\([^)]*\)\s*\{', text):
        opening = text.index('{', match.start())
        depth = 1
        end = opening + 1
        while depth:
            if text[end] == '{':
                depth += 1
            elif text[end] == '}':
                depth -= 1
            end += 1
        found[match.group(1)] = text[opening:end]
    return found


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--before', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    options = parser.parse_args()
    receipts = {'windows_tokens': [], 'shared_methods': [], 'untouched': [], 'native_only': []}
    for target in ['i686-pc-windows-msvc', 'x86_64-pc-windows-msvc', 'aarch64-pc-windows-msvc']:
        for name in ['System.cpp', 'System.h']:
            equal = selected((options.before / name).read_bytes(), target) == selected((CONSOLE / name).read_bytes(), target)
            receipts['windows_tokens'].append({'target': target, 'file': name, 'identical': equal})
            assert equal, name + ' changes Windows selected tokens'
        for name in ['PosixSystem.cpp', 'PosixSystem.h', 'TextConsolePosix.cpp', 'TextConsolePosix.h']:
            empty = not selected((CONSOLE / name).read_bytes(), target)
            receipts['native_only'].append({'target': target, 'file': name, 'empty': empty})
            assert empty, name + ' leaks native tokens into Windows'
    before = methods((options.before / 'TextConsole.cpp').read_bytes())
    after = methods((CONSOLE / 'TextConsole.cpp').read_bytes())
    assert before.keys() == after.keys()
    for name in before:
        equal = before[name] == after[name]
        approved = name in ['HandleKeyUp', 'HandleKeyDown']
        receipts['shared_methods'].append({'method': name, 'body_bytes_identical': equal, 'approved_history_exception': approved})
        assert equal or approved, 'unapproved shared method edit: ' + name
    for name in ['TextConsole.h', 'TextConsoleWin32.cpp', 'TextConsoleWin32.h']:
        data = (CONSOLE / name).read_bytes()
        equal = data == (options.before / name).read_bytes()
        receipts['untouched'].append({'file': name, 'bytes_identical': equal, 'sha256': hashlib.sha256(data).hexdigest()})
        assert equal, 'unapproved layout/backend edit: ' + name
    options.output.write_text(json.dumps(receipts, indent=2))
    print('PASS Windows selected tokens, native guards, unchanged shared methods/layout/backend; two history exceptions')


if __name__ == '__main__':
    main()
