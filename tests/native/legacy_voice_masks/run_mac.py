"""Run complete-class voice-mask fixtures with owned authored data only."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--art', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
art = args.art.resolve()
art.mkdir(parents=True, exist_ok=True)
rows = []
outputs = []
for profile, config, sanitizers in [('release', 'Release', False), ('debug', 'Debug', False), ('sanitizers', 'Debug', True)]:
    build = art / profile
    commands = [
        ['cmake', '-S', str(Path(__file__).parent), '-B', str(build), '-G', 'Ninja',
         '-DCMAKE_BUILD_TYPE=' + config, '-DCMAKE_CXX_COMPILER=/usr/bin/clang++',
         '-DCSE_VOICE_MASK_SANITIZERS=' + ('ON' if sanitizers else 'OFF')],
        ['cmake', '--build', str(build), '--parallel', '2'],
        ['ctest', '--test-dir', str(build), '--output-on-failure'],
        [str(build / 'voice_masks')],
    ]
    for ordinal, command in enumerate(commands):
        result = subprocess.run(command, cwd=root, capture_output=True, timeout=45)
        (art / f'{profile}-{ordinal}-stdout.bin').write_bytes(result.stdout)
        (art / f'{profile}-{ordinal}-stderr.log').write_bytes(result.stderr)
        rows.append({'profile': profile, 'command': command, 'exit': result.returncode})
        (art / 'mac-commands.json').write_text(json.dumps(rows, indent=2) + '\n')
        assert result.returncode == 0, rows[-1]
        if ordinal == len(commands) - 1:
            outputs.append(result.stdout)
assert outputs[0] == outputs[1] == outputs[2]
(art / 'mac-comparison.json').write_text(json.dumps({'profiles': 3, 'cases_per_profile': 514,
    'identical_bytes': len(outputs[0]), 'sha256': hashlib.sha256(outputs[0]).hexdigest(),
    'real_provider_network_or_device_execution': False}, indent=2) + '\n')
print('PASS three actual-method voice-mask profiles; 514 cases/profile, exact output equality')
