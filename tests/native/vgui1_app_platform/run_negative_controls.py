"""Reproduce measured pre-fix defects in tagged copies; never alter production."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--art', type=Path, required=True)
args = p.parse_args()
root = Path(__file__).resolve().parents[3]
source = root / 'tests/native/vgui1_app_platform'
vgui = root / 'src/utils/vgui/lib-src/vgui/posix'
art = args.art.resolve()
controls = [
    ('retain-reentry', 'VGui1AppPlatform.cpp', 'data.state = State::Binding;', 'data.state = State::Closing;', 'CSE_APP_ADAPTER_SOURCE'),
    ('unknown-pump', 'App.cpp', 'result != platform::PumpResult::Empty && result != platform::PumpResult::Quit', 'result == platform::PumpResult::Error', 'CSE_APP_PLATFORM_SOURCE'),
]
rows = []
for name, filename, old, replacement, variable in controls:
    target = art / ('CONTROL-BROKEN-' + name)
    target.mkdir(exist_ok=True)
    actual = vgui / filename
    text = actual.read_text()
    assert text.count(old) == 1
    copy = target / filename
    copy.write_text(text.replace(old, replacement))
    commands = [
        ['cmake', '-S', source, '-B', target / 'build', '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', '-D' + variable + '=' + str(copy)],
        ['cmake', '--build', target / 'build', '--parallel', '2'],
        [target / 'build/app_component']
    ]
    results = []
    for index, command in enumerate(commands):
        result = subprocess.run([str(x) for x in command], capture_output=True, text=True, timeout=120)
        (target / (str(index) + '.log')).write_text(result.stdout + result.stderr)
        results.append({'command': [str(x) for x in command], 'exit': result.returncode})
        if index < 2:
            assert result.returncode == 0, results[-1]
        else:
            assert result.returncode != 0, name
            if name == 'unknown-pump':
                assert 'unknown event result cannot report successful readiness' in result.stderr
    rows.append({'name': name, 'control_only': True, 'source': str(actual), 'source_sha256': hashlib.sha256(actual.read_bytes()).hexdigest(),
        'copy_sha256': hashlib.sha256(copy.read_bytes()).hexdigest(), 'only_change': [old, replacement], 'runs': results})
(art / 'negative-controls.json').write_text(json.dumps(rows, indent=2) + '\n')
print('PASS: both tagged pre-fix controls fail actual current component tests')
