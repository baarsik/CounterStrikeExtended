"""Build isolated C++14 ARM64/min14 formatter profiles and compare exact UCRT rows."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import subprocess
import sys

parser = argparse.ArgumentParser()
parser.add_argument('--reference', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--build-root', type=Path, required=True)
args = parser.parse_args()
source = Path(__file__).resolve().parent
args.output.mkdir(parents=True, exist_ok=False)
records = []
profiles = [('release', 'Release', []), ('debug', 'Debug', []),
            ('sanitized', 'RelWithDebInfo', ['-DCSE_WIDE_SANITIZERS=ON']),
            ('fast-release', 'Release', ['-DCSE_WIDE_FAST_FP=ON']),
            ('fast-debug', 'Debug', ['-DCSE_WIDE_FAST_FP=ON'])]
(args.output / 'host.json').write_text(json.dumps({'platform': platform.platform(), 'machine': platform.machine()},
                                                indent=2) + '\n')
for name, configuration, options in profiles:
    build = (args.build_root / name).resolve()
    output = args.output / name
    output.mkdir()
    row = {'profile': name, 'configuration': configuration, 'phases': []}
    commands = [('configure', ['cmake', '-S', str(source), '-B', str(build), '-G', 'Ninja',
                               '-DCMAKE_BUILD_TYPE=' + configuration, '-DCSE_WIDE_BUILD_HELPER=ON',
                               '-DCMAKE_OSX_ARCHITECTURES=arm64', '-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0'] + options),
                ('build', ['cmake', '--build', str(build), '--parallel', '3', '--target',
                           'wide_format_helper', 'wide_numeric_helper', 'wide_format_edge_tests',
                           'wide_format_wrapper_tests'])]
    for phase, executable in [('wide', 'wide_format_helper'), ('numeric', 'wide_numeric_helper'),
                              ('edge', 'wide_format_edge_tests'), ('wrappers', 'wide_format_wrapper_tests')]:
        commands.append((phase, [str(build / executable)]))
    commands.append(('compare', [sys.executable, str(source / 'compare_helper.py'),
                                 '--reference', str(args.reference), '--helper', str(output / 'wide'),
                                 '--numeric-helper', str(output / 'numeric'),
                                 '--output', str(output / 'comparison.json')]))
    for phase, command in commands:
        working = output / phase
        working.mkdir()
        with (working / 'stdout.log').open('wb') as stdout, (working / 'stderr.log').open('wb') as stderr:
            result = subprocess.run(command, cwd=working, stdout=stdout, stderr=stderr, timeout=180, check=False)
        row['phases'].append({'phase': phase, 'command': command, 'exit_code': result.returncode})
        if result.returncode:
            break
    if len(row['phases']) == len(commands) and row['phases'][-1]['exit_code'] == 0:
        binaries = {}
        for executable in ('wide_format_helper', 'wide_numeric_helper', 'wide_format_edge_tests',
                           'wide_format_wrapper_tests'):
            path = build / executable
            import shutil
            shutil.copy2(path, output / executable)
            binaries[executable] = {'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                'file': subprocess.check_output(['file', str(path)], text=True).strip(),
                'load_commands': subprocess.check_output(['otool', '-l', str(path)], text=True)}
        row['binaries'] = binaries
        shutil.copy2(build / 'compile_commands.json', output / 'compile_commands.json')
        shutil.copy2(build / 'liblegacy_wide_format.a', output / 'liblegacy_wide_format.a')
        (output / 'core-undefined-symbols.txt').write_bytes(subprocess.check_output(
            ['nm', '-u', str(build / 'liblegacy_wide_format.a')]))
    records.append(row)
    (args.output / 'cases.json').write_text(json.dumps(records, indent=2) + '\n')
    print(name, [(x['phase'], x['exit_code']) for x in row['phases']], flush=True)
failed = any(len(x['phases']) != 7 or x['phases'][-1]['exit_code'] for x in records)
(args.output / 'exit.txt').write_text(str(int(failed)) + '\n')
raise SystemExit(int(failed))
