"""Real four-TU launcher, real filesystem dylib, authored engine; isolated owned paths only."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def run(args):
    with tempfile.TemporaryDirectory(prefix='cse launcher bootstrap ') as temporary:
        root = Path(temporary).resolve()
        app = root / 'Application β Space'
        if args.case == 'long-path':
            app = app / ('nested ' + 'a' * 90) / ('owned ' + 'b' * 90) / ('leaf ' + 'c' * 70)
        app.mkdir(parents=True)
        if args.case == 'long-path':
            assert len(os.fsencode(app)) > 260
        cwd = root / 'unrelated cwd'
        cwd.mkdir()
        executable = app / ('cstrike' if args.case == 'alias' else 'hl')
        shutil.copy2(args.launcher, executable)
        shutil.copy2(args.probe, app / 'probe')
        for name in ['hw.dylib', 'sw.dylib']:
            shutil.copy2(args.engine, app / name)
        provider = args.mismatch if args.case == 'mismatch' else args.nullfs if args.case == 'null-interface' else args.filesystem
        shutil.copy2(provider,
                     app / 'filesystem_stdio.dylib')
        (app / 'root-marker.txt').write_text('owned-root-marker')
        log = root / 'events.tsv'
        env = dict(os.environ)
        env.update(CSE_BOOTSTRAP_LOG=str(log), CSE_BOOTSTRAP_COUNTER=str(root / 'counter'),
                   CSE_BOOTSTRAP_PLAN='restart' if args.case in ['restart', 'retained'] else 'normal')
        command = [str(executable)]
        if args.case == 'explicit-game':
            command += ['-game', 'cstrike']
        if args.case == 'software':
            command += ['-soft']
        if args.case == 'relative-argv':
            command = ['hl']
        if args.case in ['borrowed', 'retained']:
            command = [str(app / 'probe'), args.case, str(app / 'hw.dylib')]
            if args.case == 'retained':
                command.append(str(executable))
        if args.case == 'null-interface':
            command = [str(app / 'probe'), 'nullfs', str(app / 'filesystem_stdio.dylib'), str(executable)]
        if args.leaks:
            assert args.case in ['normal', 'restart', 'retained']
            command = ['/usr/bin/leaks', '--atExit', '--', *command]
        result = subprocess.run(command, executable=str(executable) if args.case == 'relative-argv' else None,
                                cwd=cwd, env=env, capture_output=True, timeout=10)
        events = log.read_text().splitlines() if log.exists() else []
        report = {'case': args.case, 'exit': result.returncode, 'stdout': result.stdout.decode(errors='replace'),
                  'stderr': result.stderr.decode(errors='replace'), 'events': events}
        print(json.dumps(report, ensure_ascii=False, indent=2))
        if args.leaks:
            assert '0 leaks for 0 total leaked bytes' in report['stdout'], report
        if args.case == 'mismatch':
            assert result.returncode == 0 and 'Incompatible owned filesystem' in report['stderr']
            assert not events, 'ABI mismatch must precede factory/engine calls'
        elif args.case == 'null-interface':
            assert result.returncode == 1 and 'did not provide VFileSystem009' in report['stderr'], report
            assert events == ['null_filesystem_factory'], events
        elif args.case == 'borrowed':
            assert result.returncode == 0, report
            assert events.count('load\t') == 1 and events.count('unload\t') == 1, events
        else:
            # leaks returns its analysis status; the direct CTest case verifies the process exit status.
            expected_exit = 1 if args.case == 'retained' and not args.leaks else 0
            assert result.returncode == expected_exit, report
            expected_runs = 2 if args.case == 'restart' else 1
            if args.case == 'retained':
                assert 'Engine image remains loaded' in report['stderr'], report
                if args.leaks:
                    assert 'probe_retained=1 actual_entry_result=1' in report['stdout'], report
            assert events.count('run\t') == expected_runs, events
            assert events.count('base\t' + str(app)) == expected_runs, events
            assert events.count('cwd\t' + str(cwd)) == expected_runs, events
            assert events.count('checked_borrowed_filesystem\tyes') == expected_runs, events
            assert events.count('root_file\tyes') == expected_runs, events
            commands = [row.split('\t', 1)[1] for row in events if row.startswith('command\t')]
            if args.case in ['explicit-game', 'alias']:
                assert '-game cstrike' in commands[0], commands
            assert events.count('load\t') == expected_runs and events.count('unload\t') == expected_runs, events


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    for key in ['launcher', 'probe', 'engine', 'mismatch', 'nullfs', 'filesystem']:
        parser.add_argument('--' + key, type=Path, required=True)
    parser.add_argument('--case', required=True)
    parser.add_argument('--leaks', action='store_true')
    run(parser.parse_args())
