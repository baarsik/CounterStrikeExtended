"""Replay the actual generated thread.cpp command for the bounded logical-ID fixture."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess


def run(args):
    database = json.loads(args.database.read_text())
    selected = [item for item in database if Path(item['file']).as_posix().endswith('/tier0/thread.cpp')]
    assert len(selected) == 1
    item = selected[0]
    original = shlex.split(item['command'])
    old_sdk = Path(item['file']).parents[1]
    source = args.sdk / 'tier0/thread.cpp'
    command = [str(source) if value == item['file'] else value.replace('-I' + str(old_sdk), '-I' + str(args.sdk))
               for value in original]
    command[command.index('-o') + 1] = str(args.output)
    command += ['-MMD', '-MF', str(args.output) + '.d']
    if args.controlled_id_provider:
        command += ['-Dpthread_mach_thread_np=CseFixtureNativeThreadId']
    if args.sanitizers:
        command += ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
    args.output.parent.mkdir(parents=True, exist_ok=True)
    receipt = {'database_sha256': hashlib.sha256(args.database.read_bytes()).hexdigest(),
               'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
               'original_command': original, 'command': command, 'sanitizers': args.sanitizers,
               'controlled_id_provider': args.controlled_id_provider}
    result = subprocess.run(command, cwd=args.output.parent)
    receipt['exit_code'] = result.returncode
    Path(str(args.output) + '.command.json').write_text(json.dumps(receipt, indent=2) + '\n')
    raise SystemExit(result.returncode)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--database', type=Path, required=True)
    parser.add_argument('--sdk', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--sanitizers', action='store_true')
    parser.add_argument('--controlled-id-provider', action='store_true')
    run(parser.parse_args())
