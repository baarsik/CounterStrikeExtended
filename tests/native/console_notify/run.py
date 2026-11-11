"""Run immutable actual-source RED or GREEN evidence (at most two compilers)."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--packet', type=Path, required=True)
parser.add_argument('--expect-red', action='store_true')
args = parser.parse_args()
packet = args.packet.resolve()
packet.mkdir(parents=True, exist_ok=False)
source = packet / 'console.cpp'
shutil.copyfile(args.source, source)
rows = []

def run(label, argv, expected=0):
    result = subprocess.run([str(arg) for arg in argv], capture_output=True, text=True)
    (packet / (label + '.log')).write_text(result.stdout + result.stderr)
    rows.append(dict(label=label, argv=[str(arg) for arg in argv], cwd=str(Path.cwd()),
                     exit=result.returncode, expectedExit=expected, log=label + '.log'))
    (packet / 'commands.json').write_text(json.dumps(rows, indent=2) + '\n')
    print(label, result.returncode, flush=True)
    if (result.returncode == 0) != (expected == 0):
        raise RuntimeError('Unexpected command result: ' + label)
    return result.stdout + result.stderr

for profile in (['release', 'asan', 'sanitizers'] if args.expect_red else ['release', 'debug', 'sanitizers']):
    build = packet / profile
    argv = ['cmake', '-S', Path(__file__).resolve().parent, '-B', build, '-G', 'Ninja',
            '-DCMAKE_BUILD_TYPE=' + ('Release' if profile == 'release' else 'Debug'),
            '-DCSE_NOTIFY_SOURCE=' + str(source)]
    if profile in ['asan', 'sanitizers']:
        argv += ['-DCSE_NOTIFY_SANITIZERS=ON']
        if profile == 'asan':
            argv += ['-DCSE_NOTIFY_SANITIZER_SET=address']
    run(profile + '-configure', argv)
    run(profile + '-build', ['cmake', '--build', build, '--parallel', '2'])
    if args.expect_red:
        run(profile + '-baseline-full-1920', [build / 'console_notify_tests', 'full-1920'])
        case = 'short-stale' if profile == 'release' else 'full-2560'
        output = run(profile + '-red-' + case, [build / 'console_notify_tests', case], expected=1)
        needle = {'release': 'only visible-prefix draws', 'asan': 'AddressSanitizer: global-buffer-overflow',
                  'sanitizers': "runtime error: index 256 out of bounds for type 'char[256]'"}[profile]
        assert needle in output, 'RED must reproduce intended behavior: ' + needle
    else:
        run(profile + '-test', ['ctest', '--test-dir', build, '--output-on-failure', '-V'])
(packet / 'receipt.json').write_text(json.dumps(dict(
    source=str(source), sourceSha256=hashlib.sha256(source.read_bytes()).hexdigest(),
    expectRed=args.expect_red, commands=rows,
    toolPins={str(Path(__file__).resolve()): hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              str(Path(__file__).with_name('extract.py').resolve()):
              hashlib.sha256(Path(__file__).with_name('extract.py').read_bytes()).hexdigest()},
    extractionPins={profile: json.loads((packet / profile / 'actual/source-pins.json').read_text())
                    for profile in (['release', 'asan', 'sanitizers'] if args.expect_red
                                    else ['release', 'debug', 'sanitizers'])}), indent=2) + '\n')
print('EXPECTED RED PRESERVED' if args.expect_red else 'ALL GREEN: 20 cases x 3 profiles', flush=True)
