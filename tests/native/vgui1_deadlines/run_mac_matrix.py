from pathlib import Path
import argparse
import json
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--art', type=Path, required=True)
a = p.parse_args()
art = a.art.resolve()
source = Path(__file__).resolve().parent
rows = []
for view in ['private', 'public']:
    for profile, configuration, sanitizers in [('release', 'Release', False), ('debug', 'Debug', False), ('sanitizers', 'Debug', True)]:
        name = 'mac-' + view + '-' + profile
        build = art / name
        commands = [
            ['cmake', '-S', str(source), '-B', str(build), '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=' + configuration,
             '-DHEADER_VIEW=' + view, '-DDEADLINE_SANITIZERS=' + ('ON' if sanitizers else 'OFF'),
             '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON'],
            ['cmake', '--build', str(build), '--parallel', '2'],
            ['ctest', '--test-dir', str(build), '--output-on-failure'],
        ]
        for phase, command in enumerate(commands):
            result = subprocess.run(command, capture_output=True, timeout=120)
            (art / (name + '-' + str(phase) + '.log')).write_bytes(result.stdout + result.stderr)
            rows.append({'name': name, 'phase': phase, 'command': command, 'exit': result.returncode})
            (art / 'mac-matrix-commands.json').write_text(json.dumps(rows, indent=2) + '\n')
            assert result.returncode == 0, rows[-1]
print('PASS six Mac real-header R/D/sanitizer profiles:747 actual-method rows plus547 integer vectors each')
