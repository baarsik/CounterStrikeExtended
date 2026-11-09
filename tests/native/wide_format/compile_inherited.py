"""Replay actual selected Mac compiler policy with only helper input/output substituted."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--artifacts', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=False)
root = Path(__file__).resolve().parents[3]
helper = root / 'GoldSrc-SDK/common/legacy_wide_format.cpp'
records = []
for configuration in ('release', 'debug'):
    database = args.artifacts / ('macos-runtime-policy-' + configuration) / 'compile_commands.json'
    rows = json.loads(database.read_text())
    for owner, suffix in [('engine', 'engine/KeyValues.cpp'), ('client', 'cstrike/game_shared/shared_util.cpp')]:
        original = next(x for x in rows if x['file'].endswith(suffix))
        for fast in (False, True):
            name = configuration + '-' + owner + ('-fast' if fast else '-selected')
            output = args.output / name
            output.mkdir()
            command = shlex.split(original['command'])
            command[command.index('-o') + 1] = str(output / 'formatter.o')
            command[command.index('-c') + 1] = str(helper)
            if fast:
                command = [x for x in command if not x.startswith('-ffp-model=') and not x.startswith('-ffp-contract=')]
                command.append('-ffp-model=fast')
            command.extend(['-MD', '-MF', str(output / 'formatter.d')])
            with (output / 'stdout.log').open('wb') as stdout, (output / 'stderr.log').open('wb') as stderr:
                result = subprocess.run(command, cwd=original['directory'], stdout=stdout, stderr=stderr,
                                        timeout=60, check=False)
            row = {'case': name, 'database_sha256': hashlib.sha256(database.read_bytes()).hexdigest(),
                   'original': original, 'command': command, 'exit_code': result.returncode}
            if result.returncode == 0:
                dependency = (output / 'formatter.d').read_text().replace('\\\n', '')
                paths = shlex.split(dependency.partition(':')[2])
                row['header_inputs'] = {x: hashlib.sha256(Path(x).read_bytes()).hexdigest() for x in paths}
                row['object_sha256'] = hashlib.sha256((output / 'formatter.o').read_bytes()).hexdigest()
            records.append(row)
            print(name, result.returncode, flush=True)
(args.output / 'cases.json').write_text(json.dumps(records, indent=2) + '\n')
raise SystemExit(int(any(x['exit_code'] for x in records)))
