#!/usr/bin/env python3
"""Stage unchanged real declaring context, arithmetic and original controlled bindings."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--repo', type=Path, required=True)
parser.add_argument('--original', type=Path, required=True)
parser.add_argument('--art', type=Path, required=True)
args = parser.parse_args()
repo = args.repo.resolve()
original = args.original.resolve()
art = args.art.resolve()
art.mkdir(parents=True, exist_ok=True)
manifest = original / 'FINAL_SHA256.json'
assert hashlib.sha256(manifest.read_bytes()).hexdigest() == '4b479bc64e734149cc59ca39f579a79e8f81ad1753d15a2419a647838846503b'
source = repo / 'src/vgui2/src/System.cpp'
before = (original / 'owning-inputs/src/vgui2/src/System.cpp').read_bytes()
assert hashlib.sha256(before).hexdigest() == '52f95757ab48a39b983c99b9a68b02786d99f35e6aebdf430023e9210dbbb7e7'
after = source.read_bytes()
include = b'#if defined(_WIN64)\r\n#include "../../common/legacy_double_to_int.h"\r\n#endif\r\n\r\n'
body = b'#if defined(_WIN64)\r\n    constexpr double MillisecondsPerSecond = 1000.0;\r\n    return static_cast<long>(CseLegacyTruncateDoubleToInt(GetCurrentTime() * MillisecondsPerSecond));\r\n#else\r\n\treturn (long)( GetCurrentTime() * 1000.0 );\r\n#endif'
legacy = b'\treturn (long)( GetCurrentTime() * 1000.0 );'
assert after.count(include) == after.count(body) == 1
assert after.replace(include, b'').replace(body, legacy) == before, 'Unexpected source changes outside approved two blocks'
assert after.count(b'\n') == after.count(b'\r\n'), 'Preserve original CRLF encoding'
(art / 'System-before.cpp').write_bytes(before)
(art / 'System-after.cpp').write_bytes(after)
stage = art / 'windows-source'
imports = stage / 'src/vgui2/src'
imports.mkdir(parents=True, exist_ok=True)
extractor = original / 'source/tests/native/vgui2_system_clock/extract_reference.py'
subprocess.run(['python3', str(extractor), '--source', str(source), '--output', str(imports)], check=True)
prior = json.loads((original / 'windows-source/imports/imports.json').read_text())
current = json.loads((imports / 'imports.json').read_text())
assert prior['actual_class_sha256'] == current['actual_class_sha256']
assert all(prior['methods'][key] == current['methods'][key] for key in ['constructor', 'destructor', 'current'])
assert prior['methods']['millis'] != current['methods']['millis']
(imports / 'actual_numeric_include.inc').write_bytes(include.replace(b'\r\n', b'\n'))
helper = repo / 'src/common/legacy_double_to_int.h'
assert helper.read_bytes() == (original / 'component-source/src/common/legacy_double_to_int.h').read_bytes()
target = stage / 'src/common/legacy_double_to_int.h'
target.parent.mkdir(parents=True, exist_ok=True)
shutil.copy2(helper, target)
fixture = (original / 'source/tests/native/vgui2_system_clock/windows_reference.cpp').read_text()
needle = '#include "vgui/ISystem.h"\n'
assert fixture.count(needle) == 1
(stage / 'windows_reference.cpp').write_text(fixture.replace(needle, needle + '#include "actual_numeric_include.inc"\n'))
shutil.copy2(original / 'source/tests/native/vgui2_system_clock/clock_fp_observation.h', stage)
shutil.copy2(repo / 'tests/native/vgui2_system_millis/run_windows.py', stage)
(art / 'source-scope.json').write_text(json.dumps({'original_source_sha256': hashlib.sha256(before).hexdigest(),
    'current_source_sha256': hashlib.sha256(after).hexdigest(), 'only_approved_include_and_getter_changed': True,
    'Win32_original_else_line_byte_identical': True, 'class_constructor_destructor_current_body_unchanged': True,
    'CRLF_lines': after.count(b'\r\n'), 'native64_guard': '_WIN64', 'Mac_System_integration_changed': False}, indent=2) + '\n')
print('PASS exact two-block source scope, original Win32 bytes, actual declaring class/non-getter method hashes')
