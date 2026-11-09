"""Compile and execute real core/body mutants against controlled fixtures only."""
from pathlib import Path
import argparse
import json
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
args.output.mkdir(parents=True, exist_ok=False)
core = root / 'GoldSrc-SDK/common/native_mouse_poller.h'
original = core.read_text()
variants = {
    'mixed-pair': ('delta.y = static_cast<int>(pendingY_);', 'delta.y = static_cast<int>(pendingX_);'),
    'drop-ordinary-pending': ('if (!Suspend(false, deadline))', 'if (!Suspend(true, deadline))'),
    'retain-old-owner-queue': ('if (discard) {', 'if (false && discard) {'),
    'skip-quiescence': ('while (inFlight_) {', 'while (false && inFlight_) {'),
    'skip-prewarp-capture': ('return Add(point);', 'return Usable();'),
    'close-live-worker': ('if (started) {', 'if (false && started) {'),
    'overwrite-first-failure': ('if (first_.kind == FailureKind::None) {', 'if (true) {'),
}
results = []
for name, (before, after) in variants.items():
    assert original.count(before) == 1, name
    case = args.output / name
    include = case / 'include'
    include.mkdir(parents=True)
    (include / core.name).write_text(original.replace(before, after))
    image = case / 'test'
    command = ['c++', '-std=c++14', '-O2', '-ffp-model=precise', '-ffp-contract=off', '-pthread',
               '-I' + str(include), '-I' + str(root / 'tests/native/mouse_polling'),
               str(root / 'tests/native/mouse_polling/core_tests.cpp'), '-o', str(image)]
    build = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (case / 'compile.log').write_bytes(build.stdout)
    assert build.returncode == 0, (name, build.stdout)
    try:
        run = subprocess.run([str(image)], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=8)
        code, output = run.returncode, run.stdout
    except subprocess.TimeoutExpired as error:
        code, output = 'timeout', error.stdout or b''
    (case / 'run.log').write_bytes(output)
    assert code != 0, name
    results.append({'mutant': name, 'command': command, 'exit': code})
(args.output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
print('rejected real-core mutants:', len(results))
