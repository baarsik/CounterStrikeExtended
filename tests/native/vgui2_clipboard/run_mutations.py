#!/usr/bin/env python3
import argparse
import json
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--repo', type=Path, required=True)
parser.add_argument('--art', type=Path, required=True)
args = parser.parse_args()
repo = args.repo.resolve()
root = args.art.resolve() / 'mutations'
root.mkdir(exist_ok=True)
source = repo / 'src/vgui2/src'
original = (source / 'NativeClipboard.cpp').read_text()
controls = [
    ('count-crosses-view', 'outCount = static_cast<std::int32_t>(snapshot.legacyTextStorageBytes);',
        'outCount = static_cast<std::int32_t>(snapshot.unicodeUnitExtentBytes);'),
    ('raw-becomes-prefix-only',
        'const std::uint64_t copied = remaining < destination.storageBytes ? remaining : destination.storageBytes;',
        'const std::uint64_t copied = remaining == 0 ? 0 : 1;'),
    ('unit-offset-ignored', 'ReadUnit(source, offset + index)', 'ReadUnit(source, index)'),
    ('missing-NUL-accepted', 'if (!terminated) {',
        'if (!terminated && snapshot.generation == UINT64_MAX) {'),
    ('packing-stops-at-NUL',
        'const auto value = static_cast<std::uint32_t>(source.data[static_cast<std::size_t>(index)]);',
        'const auto value = static_cast<std::uint32_t>(source.data[static_cast<std::size_t>(index)]);\n'
        '        if (value == 0) { break; }'),
    ('raw-surrogates-repaired', 'WriteUnit(destination.data, position, value);',
        'WriteUnit(destination.data, position, value >= 0xd800 && value <= 0xdfff ? 0xfffd : value);'),
    ('invalid-scalar-accepted', 'return Status::InvalidScalar;', 'continue;'),
    ('failed-count-mutated',
        'if (!snapshot.hasLegacyTextCount) {\n        return Status::MissingFormat;',
        'if (!snapshot.hasLegacyTextCount) {\n        outCount = 0;\n        return Status::MissingFormat;'),
    ('physical-capacity-rounded-up', 'destination.storageBytes / sizeof(wchar_t)',
        '(destination.storageBytes + sizeof(wchar_t) - 1) / sizeof(wchar_t)'),
    ('wrong-byte-terminator-plan', 'static_cast<std::uint64_t>(textLength) + 1;',
        'static_cast<std::uint64_t>(textLength) + 2;'),
]
receipts = []
for name, before, after in controls:
    assert original.count(before) == 1, (name, original.count(before))
    work = root / name
    work.mkdir(exist_ok=True)
    (work / 'NativeClipboard.cpp').write_text(original.replace(before, after))
    shutil.copy2(source / 'NativeClipboard.h', work / 'NativeClipboard.h')
    exe = work / 'fixture'
    command = ['/usr/bin/clang++', '-std=c++14', '-O2', '-Wall', '-Wextra', '-Werror', '-Wconversion',
        '-pedantic', '-I' + str(work), str(repo / 'tests/native/vgui2_clipboard/clipboard_tests.cpp'),
        str(work / 'NativeClipboard.cpp'), '-o', str(exe)]
    result = subprocess.run(command, capture_output=True, timeout=90)
    (work / 'compile.log').write_bytes(result.stdout + result.stderr)
    assert result.returncode == 0, (name, result.stderr)
    result = subprocess.run([str(exe)], capture_output=True, timeout=30)
    (work / 'stdout').write_bytes(result.stdout)
    (work / 'stderr').write_bytes(result.stderr)
    assert result.returncode == 1 and b'FAIL ' in result.stderr, (name, result.returncode, result.stderr)
    receipts.append({'name': name, 'compile_command': command, 'compile_exit': 0,
        'test_exit': result.returncode, 'semantic_failure': result.stderr.decode().strip()})
    print('PASS rejected', name)
(root / 'receipts.json').write_text(json.dumps(receipts, indent=2) + '\n')
