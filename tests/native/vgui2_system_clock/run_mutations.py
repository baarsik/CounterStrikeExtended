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
art = args.art.resolve()
root = art / 'mutations'
root.mkdir(exist_ok=True)
original = (repo / 'src/vgui2/src/NativeSystemClock.h').read_text()
controls = [
    ('backward-threshold', 'state.oldTime - sample < MaximumIgnoredBackwardStep',
        'state.oldTime - sample <= MaximumIgnoredBackwardStep', 'normal', '1000000'),
    ('first-sample', 'state.oldTime = sample;\n        state.first = false;',
        'state.oldTime = sample;\n        state.first = false;\n        state.currentTime = 1.0;', 'normal', '1000000'),
    ('wide-wrap', 'const std::uint32_t delta = sample - state.oldTime;',
        'const std::uint64_t delta = static_cast<std::uint64_t>(sample) - state.oldTime;', 'seeded', '1000000000'),
    ('fudge-boundary', 'state.sameTimeCount > SameTimeLimit',
        'state.sameTimeCount >= SameTimeLimit', 'same-time', '1000000'),
    ('float-precision', 'CseLegacyTruncateDoubleToInt(seconds * MillisecondsPerSecond)',
        'CseLegacyTruncateDoubleToInt(static_cast<float>(seconds * MillisecondsPerSecond))', 'millis', '1000000'),
    ('zero-frequency', 'if (frequency == 0) {\n        return false;',
        'if (frequency == 0) {\n        frequency = 1;', 'boundaries', None),
    ('nanosecond-overflow', 'if (positiveSeconds > (MaximumCounter - positiveNanoseconds) / NanosecondsPerSecond)',
        'if (positiveSeconds > MaximumCounter)', 'boundaries', None),
]
receipts = []
for name, before, after, scenario, frequency in controls:
    assert original.count(before) == 1, name
    work = root / name
    source = work / 'src/vgui2/src'
    common = work / 'src/common'
    source.mkdir(parents=True, exist_ok=True)
    common.mkdir(parents=True, exist_ok=True)
    (source / 'NativeSystemClock.h').write_text(original.replace(before, after))
    shutil.copy2(repo / 'src/common/legacy_double_to_int.h', common / 'legacy_double_to_int.h')
    exe = work / 'fixture'
    command = ['/usr/bin/clang++', '-std=c++14', '-O2', '-ffp-contract=off', '-Wall', '-Wextra', '-Werror',
        '-I' + str(source), str(repo / 'tests/native/vgui2_system_clock/clock_fixture.cpp'), '-o', str(exe)]
    result = subprocess.run(command, capture_output=True, timeout=60)
    (work / 'compile.log').write_bytes(result.stdout + result.stderr)
    assert result.returncode == 0, (name, result.stderr.decode())
    invocation = [str(exe), scenario] + ([frequency] if frequency else [])
    result = subprocess.run(invocation, capture_output=True, timeout=30)
    (work / 'stdout').write_bytes(result.stdout)
    (work / 'stderr').write_bytes(result.stderr)
    detected = result.returncode != 0
    if not detected:
        expected = list(map(json.loads, (art / 'windows' / ('x86-Release-' + scenario + '-' + frequency + '.jsonl')).read_text().splitlines()))
        actual = list(map(json.loads, result.stdout.splitlines()))
        detected = len(expected) != len(actual) or any(any(a[key] != b[key]
            for key in ['row', 'counter', 'seconds_bits', 'millis']) for a, b in zip(expected, actual))
    assert detected, name
    receipts.append({'name': name, 'compile_command': command, 'compile_exit': 0,
        'execute_command': invocation, 'execute_exit': result.returncode, 'rejected_by_actual_reference_or_boundary': True})
    print('PASS rejected', name)
(root / 'receipts.json').write_text(json.dumps(receipts, indent=2) + '\n')
