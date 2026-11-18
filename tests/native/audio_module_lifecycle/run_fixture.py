"""Diagnostic spike: real DLL unload with bounded old-path deadlock evidence."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import os
import signal

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--probe', type=Path, required=True)
parser.add_argument('--module', type=Path, required=True)
parser.add_argument('--track', type=Path, required=True)
parser.add_argument('--evidence', type=Path, required=True)
parser.add_argument('--native-ownership', action='store_true', help='Expect fatal Run exceptions for a 64-bit target')
args = parser.parse_args()
args.evidence.mkdir(parents=True, exist_ok=False)
records = []
PROBE_TIMEOUT_SECONDS = 10
WINDOWS_CLEANUP_FATAL_EXIT = 5023  # ERROR_INVALID_STATE from the Windows SDK.
failures = ['none', 'cpp', 'partial', 'primed', 'inflight', 'mutex-fail', 'thread-fail', 'join-fail', 'controller-init-cpp']
failures += ['seh', 'event-fail', 'controller-init-seh', 'controller-close-seh'] if os.name == 'nt' else ['attribute-fail', 'attribute-type-fail', 'condition-fail']
cases = [(mode, failure, reference) for mode in ('effects', 'music', 'both')
         for failure in failures for reference in ('owned', 'external')]
for mode, failure, reference in cases:
    identity = f'{mode}-{failure}-{reference}'
    command = [str(args.probe.resolve()), str(args.module.resolve()), mode, failure,
               str(args.track.resolve()), 'explicit', identity, reference]
    timed_out = False
    try:
        result = subprocess.run(command, capture_output=True, timeout=PROBE_TIMEOUT_SECONDS)
        output = result.stdout
        errors = result.stderr
        code = result.returncode
    except subprocess.TimeoutExpired as error:
        # Only an owned failing diagnostic child is killed by subprocess here.
        # Passing cleanup must join normally; no worker termination is used.
        timed_out = True
        output = error.stdout or b''
        errors = error.stderr or b''
        code = None
    (args.evidence / (identity + '.stdout.log')).write_bytes(output)
    (args.evidence / (identity + '.stderr.log')).write_bytes(errors)
    if args.native_ownership and failure in ('cpp', 'seh', 'controller-init-cpp', 'controller-init-seh',
                                              'controller-close-seh'):
        passed = (not timed_out and code != 0 and b'fixture_after_unload' not in output and
                  b'Fatal native engine exception; unknown partial ownership' in errors)
    elif failure in ('join-fail', 'controller-init-cpp', 'controller-init-seh', 'controller-close-seh'):
        fatal_code = WINDOWS_CLEANUP_FATAL_EXIT if os.name == 'nt' else -signal.SIGABRT
        passed = not timed_out and code == fatal_code and (b'fixture_audio_started' in output or failure.startswith('controller-init'))
        passed = passed and b'fixture_after_unload' not in output and b'Fatal: owned audio cleanup failed' in errors
    else:
        passed = not timed_out and code == 0 and b'fixture_after_unload' in output
    records.append({'case': identity, 'exit_code': code, 'timed_out': timed_out, 'passed': passed})
    (args.evidence / 'cases.json').write_text(json.dumps(records, indent=2) + '\n')
    print(identity, 'observed' if passed else 'FAILED', flush=True)
inputs = {str(path): hashlib.sha256(path.read_bytes()).hexdigest()
          for path in (args.probe, args.module, args.track)}
(args.evidence / 'images.json').write_text(json.dumps(inputs, indent=2) + '\n')
raise SystemExit(0 if all(record['passed'] for record in records) else 1)
