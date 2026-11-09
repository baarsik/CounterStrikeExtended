#!/usr/bin/env python3
"""Compile controlled source copies; every deliberate contract break must be caught."""
import argparse
import json
import resource
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--art', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
art = args.art.resolve() / 'negative-controls'
art.mkdir(parents=True, exist_ok=True)
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
source = (root / 'GoldSrc-SDK/engine/VGui1NativeFontSelection.cpp').read_text()
controls = {
    'passive-query-cannot-admit': ('const auto prepared = CseVgui1FontPrepareV1',
        'const auto prepared = CseVgui1FontQueryV1'),
    'nonready-must-invalidate': ('if (result != Result::Ready || serial_ == (std::numeric_limits<std::uint64_t>::max)()) {\n        atlasValid_ = false;',
        'if (result != Result::Ready || serial_ == (std::numeric_limits<std::uint64_t>::max)()) {'),
    'renderer-must-invalidate': ('catalog_ != status.catalogGeneration || renderer_ != rendererGeneration',
        'catalog_ != status.catalogGeneration'),
    'catalog-must-invalidate': ('catalog_ != status.catalogGeneration || renderer_ != rendererGeneration',
        'renderer_ != rendererGeneration'),
    'failed-unpin-retains-ownership': ('state_ = State::ReleasePending;\n        return Result::ReleaseFailed;',
        'state_ = State::Retired;\n        return Result::Retired;'),
    'pin-in-progress-is-busy': ('return state_ == State::Empty || state_ == State::Retired ? Result::Retired : Result::Busy;',
        'return Result::Retired;'),
    'serial-outlives-retirement': ('renderer_ = 0;\n    state_ = State::Retired;',
        'renderer_ = 0;\n    serial_ = 0;\n    state_ = State::Retired;'),
    'all-default-fonts-required': ('vgui::Scheme::sf_primary3, vgui::Scheme::sf_secondary1',
        'vgui::Scheme::sf_primary3, vgui::Scheme::sf_primary1'),
}
rows = []
for name, (before, after) in controls.items():
    assert source.count(before) == 1, (name, source.count(before))
    directory = art / name
    directory.mkdir(parents=True, exist_ok=True)
    changed = directory / 'VGui1NativeFontSelection.cpp'
    changed.write_text(source.replace(before, after, 1))
    build = directory / 'build'
    commands = [
        ['cmake', '-S', root / 'tests/native/vgui1_font_selection', '-B', build, '-G', 'Ninja',
            '-DCMAKE_BUILD_TYPE=Release', '-DSELECTION_SOURCE=' + str(changed)],
        ['cmake', '--build', build, '--parallel', '3'],
        ['ctest', '--test-dir', build, '--output-on-failure'],
    ]
    for phase, command in zip(('configure', 'build', 'test'), commands):
        result = subprocess.run([str(value) for value in command], cwd=root, capture_output=True, text=True, timeout=120)
        (directory / f'{phase}.log').write_text(result.stdout + result.stderr)
        rows.append({'control': name, 'phase': phase, 'command': [str(value) for value in command], 'exit': result.returncode})
        (art / 'commands.json').write_text(json.dumps(rows, indent=2) + '\n')
        if phase == 'test':
            assert result.returncode != 0, f'contract break escaped: {name}'
            assert 'FAIL:' in result.stdout + result.stderr, (name, result.stdout, result.stderr)
        else:
            assert result.returncode == 0, (name, phase, result.stdout, result.stderr)
    print(f'PASS rejected {name}', flush=True)
print('PASS all 8 behavioral mutation controls reject')
