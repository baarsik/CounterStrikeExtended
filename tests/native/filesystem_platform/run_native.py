"""Capture and check exact diagnostic/output bytes of bounded owned fixtures."""
import argparse
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('binary', type=Path)
parser.add_argument('scenario')
parser.add_argument('directory', type=Path)
parser.add_argument('--scalar', action='store_true')
args = parser.parse_args()
args.directory.mkdir(parents=True, exist_ok=True)
if args.scenario == 'sizes':
    stale = args.directory / 'sparse.tmp'
    if stale.exists():
        assert stale.stat().st_blocks * 512 <= 1024 * 1024, 'Unexpected allocation in owned stale sparse fixture'
        stale.unlink()
for name in ('stdout.bin', 'stderr.bin', 'normalized-diagnostics.txt'):
    (args.directory / name).unlink(missing_ok=True)
command = [str(args.binary), args.scenario, str(args.directory)] if args.scalar else [str(args.binary), str(args.directory), args.scenario]
completed = subprocess.run(command, capture_output=True, timeout=15)
(args.directory / 'stdout.bin').write_bytes(completed.stdout)
(args.directory / 'stderr.bin').write_bytes(completed.stderr)
assert completed.returncode == 0, (completed.returncode, completed.stderr.decode(errors='replace'))
if args.scenario == 'warning':
    expected = (b'FS:  Tried to Close NULL file handle!\n' + b'owned path 100%ready%n.txt, number=42\n') * 4
    expected += b'verbose warning\n' + b'x' * 4095 + b'filesystem_platform_body_passed=1\n'
    assert completed.stdout == expected
    assert completed.stderr == b''
if args.scenario == 'diagnostics':
    text = completed.stderr.decode('utf-8')
    prefix = str(args.directory)
    level = 4294967295
    entries = [line.partition('=')[2] for line in completed.stdout.decode('utf-8').splitlines()
               if line.startswith('fixture_dir_entry=')]
    assert entries, 'Independent native directory reference must be nonempty'
    expected = f'({level}) Descend: {prefix}/missing 100%n/leaf.txt, (/missing 100%n/leaf.txt), false \n'
    expected += ''.join(f'\t({level}) comparing {entry} with missing 100%n\n' for entry in entries)
    expected += f"({level}) readdir failed to find 'missing 100%n' in '{prefix}/'\n"
    assert text == expected, (text, expected)
    normalized = text.replace(str(args.directory), '<OWNED_ROOT>')
    (args.directory / 'normalized-diagnostics.txt').write_text(normalized)
print('exact_output_and_exit_passed=1 scenario=' + args.scenario)
