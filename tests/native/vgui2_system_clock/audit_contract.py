#!/usr/bin/env python3
"""Record the current interface/owner mismatch and unmodified deadline call sites."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--repo', type=Path, required=True)
parser.add_argument('--art', type=Path, required=True)
args = parser.parse_args()
repo = args.repo.resolve()
art = args.art.resolve()
out = art / 'contract-audit'
out.mkdir(exist_ok=True)
sdk = repo / 'src'
commands = []
headers = ['engine/vgui/ISystem.h', 'vgui2/src/public/vgui/ISystem.h',
    'vgui2/src/public/ISystem.h', 'cstrike/public/vgui/ISystem.h']
for index, header in enumerate(headers):
    for expected_long in [False, True]:
        tag = str(index) + ('-long-red' if expected_long else '-int32-green')
        command = ['/usr/bin/clang++', '-std=c++14', '-fsyntax-only', '-Wall', '-Wextra', '-Werror',
            '-I' + str(sdk / 'vgui2/src/public'), '-I' + str(sdk / 'public'),
            '-DCSE_CLOCK_SYSTEM_HEADER="' + str(sdk / header) + '"']
        if expected_long:
            command.append('-DCSE_CLOCK_EXPECT_SELECTED_LONG')
        command.append(str(repo / 'tests/native/vgui2_system_clock/interface_boundary.cpp'))
        result = subprocess.run(command, capture_output=True, timeout=60)
        (out / (tag + '.log')).write_bytes(result.stdout + result.stderr)
        commands.append({'header': header, 'command': command, 'exit': result.returncode})
        assert (result.returncode != 0) == expected_long, tag
        if expected_long:
            assert b'static assertion failed' in result.stderr

selected = sdk / 'vgui2/src/System.cpp'
text = selected.read_bytes().decode('latin1')
assert re.search(r'long\s+CSystem::GetTimeMillis\s*\(\s*\)', text)
assert re.search(r'virtual\s+long\s+GetTimeMillis\s*\(\s*\)\s+override', text)
paths = ['vgui2/src/VGui.cpp']
for owner in ['vgui2', 'cstrike/vgui2']:
    paths.extend(owner + '/controls/' + name + '.cpp' for name in [
        'ScrollBar', 'TextEntry', 'Tooltip', 'AnimatingImagePanel'])
calls = []
deadlines = []
for relative in paths:
    path = sdk / relative
    lines = path.read_bytes().decode('latin1').splitlines()
    for number, line in enumerate(lines, 1):
        if 'GetTimeMillis' in line:
            calls.append({'path': relative, 'line': number, 'source': line.strip()})
        if re.search(r'(?:_scrollDelay|_cursorNextBlinkTime|_delay|m_iNextFrameTime|nexttick)\s*=', line):
            if '+' in line and ('GetTimeMillis' in line or re.search(r'\+\s*(?:now|tick->interval)|time\s*\+', line)):
                deadlines.append({'path': relative, 'line': number, 'source': line.strip()})
assert len(deadlines) == 14, deadlines
assert not any(call['path'] == 'vgui2/src/VGui.cpp' for call in calls), calls
report = {'existing_native_interface_bytes': 4, 'selected_owner_uses_long': True, 'mac_long_bytes': 8,
    'existing_native_owner_integration_qualified': False, 'helper_is_not_a_System_override': True,
    'selected_system_sha256': hashlib.sha256(selected.read_bytes()).hexdigest(),
    'headers': {relative: hashlib.sha256((sdk / relative).read_bytes()).hexdigest() for relative in headers},
    'getter_call_sites_in_deadline_owners': calls, 'unchanged_signed32_deadline_additions': deadlines,
    'vgui_event_deadlines': 'Provider ticks and delayed messages use GetCurrentTime seconds',
    'required_later_work': ['Change native selected declaring owner/definition coherently with existing signed32 interface',
        'Own one state per image and serialize actual reader plus Advance under its lifetime guard',
        'Diagnose reader/initialization failures before any legacy time value is fabricated',
        'Qualify the remaining 14 control signed32 deadline additions and unchanged signed comparisons/order independently',
        'Qualify actual monotonic clock source/frequency/sleep/lifecycle before selection']}
(out / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
(out / 'audit.json').write_text(json.dumps(report, indent=2) + '\n')
print('PASS 4 actual header signed32 probes; 4 actual long signature RED controls; 14 control deadline additions retained; core scheduler uses wide time')
