"""Run one bounded dummy or explicitly Root-authorized actual-default output case."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time

parser = argparse.ArgumentParser()
parser.add_argument('--graph', type=Path, required=True)
parser.add_argument('--fixture', type=Path, required=True)
parser.add_argument('--profile', choices=['Release', 'Debug'], required=True)
parser.add_argument('--evidence', type=Path, required=True)
parser.add_argument('--instrumented', action='store_true')
parser.add_argument('--inject-put-failure', action='store_true')
parser.add_argument('--root-authorized-live-default', action='store_true')
args = parser.parse_args()
assert not args.evidence.exists(), 'Never overwrite a prior evidence directory'
args.evidence.mkdir(parents=True)
runtime = args.graph.resolve() / 'runtime' / args.profile
binary = args.fixture.resolve() / ('a3d_live_instrumented' if args.instrumented else 'a3d_live_bare')
module = runtime / 'a3dapi.dylib'
sdl = runtime / 'libSDL3.0.dylib'
mpg = runtime / 'libmpg123.dylib'
track = args.graph.resolve() / 'dependencies/mpg123/source/mpg123-1.33.7/src/tests/sweep.mp3'
assert not args.inject_put_failure or args.instrumented
mode = '--root-authorized-live-default' if args.root_authorized_live_default else '--dummy'
env = dict(os.environ, CMAKE_BUILD_PARALLEL_LEVEL='1', CTEST_PARALLEL_LEVEL='1')
if args.root_authorized_live_default:
    assert env.get('CSE_ROOT_LIVE_OUTPUT_AUTHORIZED') == '1', 'Root live admission is required'
    assert env.get('SDL_AUDIO_DRIVER') != 'dummy', 'Live and dummy evidence must stay distinct'
else:
    env['SDL_AUDIO_DRIVER'] = 'dummy'
command = [str(binary), mode, str(module), str(sdl), str(track), str(args.evidence.resolve())]
if args.inject_put_failure:
    command.append('--inject-put-failure')
paths = [binary, module, sdl, mpg, track]
before = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}
start = time.time()
with (args.evidence / 'probe.log').open('w') as log:
    try:
        process = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, env=env, timeout=20)
        code = process.returncode
    except subprocess.TimeoutExpired:
        code = 124
        log.write('\nTIMEOUT: bounded process terminated; no automatic retry or device intervention\n')
after = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}
receipt = {'argv': command, 'mode': mode, 'instrumented': args.instrumented,
           'injected_transport_failure': args.inject_put_failure, 'exit': code,
           'seconds': time.time() - start, 'SDL_AUDIO_DRIVER': env.get('SDL_AUDIO_DRIVER'),
           'files_before': before, 'files_after': after, 'unchanged': before == after,
           'physical_audibility': 'UNQUALIFIED', 'physical_removal': 'UNRUN',
           'public_guid_selected_api': 'OPEN: no public module token producer'}
(args.evidence / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
print(args.evidence, code)
assert before == after
sys.exit(code)
