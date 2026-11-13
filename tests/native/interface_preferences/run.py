#!/usr/bin/env python3
"""Compile the actual reader/backend with a disposable copied-header CFPreferences domain."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import uuid

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--root', type=Path, default=Path('.'))
parser.add_argument('--evidence', type=Path, required=True)
args = parser.parse_args()
root = args.root.resolve()
out = args.evidence.resolve()
out.mkdir(parents=True, exist_ok=True)
if sys.platform != 'darwin':
    raise SystemExit('This native fixture requires macOS CoreFoundation')
source = out / 'source'
source.mkdir(exist_ok=True)
names = ('interface_language.h', 'interface_language_catalogue.h', 'interface_language_preference.h',
         'interface_language_preference.cpp', 'mac_settings_store.h', 'mac_settings_store.cpp')
missing = [name for name in names if not (root / 'src/common' / name).exists()]
if missing:
    raise SystemExit('FAIL: missing preference reader feature: ' + ', '.join(missing))
domain = 'org.counterstrikeextended.fixture.interface.' + uuid.uuid4().hex
for name in names:
    shutil.copyfile(root / 'src/common' / name, source / name)
header = source / 'mac_settings_store.h'
header.write_text(header.read_text().replace('org.counterstrikeextended.settings', domain))
manifest = {'domain': domain, 'sources': {
    name: hashlib.sha256((root / 'src/common' / name).read_bytes()).hexdigest() for name in names}}
(out / 'source-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
subprocess.run(['python3', str(root / 'tools/build_localization.py'),
                '--source', str(root / 'src/deploy/localization'), '--output', str(out / 'runtime'),
                '--language-header', str(source / 'InterfaceLanguages.generated.inc')], check=True)
executable = out / 'reader_tests'
subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-g', '-O1',
                '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-I', str(source),
                str(root / 'tests/native/interface_preferences/reader_tests.cpp'),
                str(source / 'interface_language_preference.cpp'), str(source / 'mac_settings_store.cpp'),
                '-framework', 'CoreFoundation', '-o', str(executable)], check=True)
windows_executable = out / 'windows_reader_tests'
subprocess.run(['c++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-g', '-O1',
                '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-D_WIN32',
                '-I', str(root / 'tests/native/interface_preferences/windows_api'), '-I', str(source),
                str(root / 'tests/native/interface_preferences/windows_reader_tests.cpp'),
                str(source / 'interface_language_preference.cpp'), '-o', str(windows_executable)], check=True)
subprocess.run([str(windows_executable)], check=True)
try:
    subprocess.run([str(executable)], check=True)
finally:
    subprocess.run([str(executable), 'cleanup'], check=True)
