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
        for name in ['hw.dylib', 'swds.dylib']:
            shutil.copy2(args.engine, app / name)
        provider = args.mismatch if args.case == 'mismatch' else args.nullfs if args.case == 'null-interface' else args.filesystem
        shutil.copy2(provider,
                     app / 'filesystem_stdio.dylib')
        (app / 'root-marker.txt').write_text('owned-root-marker')
        log = root / 'events.tsv'
        env = dict(os.environ)
        env.update(CSE_BOOTSTRAP_LOG=str(log), CSE_BOOTSTRAP_COUNTER=str(root / 'counter'),
                   CSE_BOOTSTRAP_PLAN='restart' if args.case in
                   ['restart', 'retained', 'restart-options', 'restart-software', 'restart-to-software', 'restart-to-hardware', 'restart-metal-to-gl', 'restart-gl-to-metal', 'restart-exec-failure', 'menu-software', 'menu-to-software', 'menu-to-hardware', 'menu-metal-to-gl', 'menu-gl-to-metal', 'command-size-only', 'command-renderer-only'] else 'normal')
        command = [str(executable)]
        if args.case.startswith('command-'):
            command += ['-soft', '-windowed', '-w', '800', '-h', '600']
            env['CSE_BOOTSTRAP_MENU_RENDERER'] = '@' + args.case.removeprefix('command-')
        if args.case.startswith('menu-'):
            command += ['-gl' if args.case == 'menu-to-software' else '-soft', '-windowed', '-w', '800', '-h', '600']
            env['CSE_BOOTSTRAP_MENU_RENDERER'] = 'OpenGL' if args.case == 'menu-to-hardware' else 'Software'
            if args.case == 'menu-software':
                env['CSE_BOOTSTRAP_MENU_RESOLUTION'] = '800 x 600'
        if args.case in ['menu-metal-to-gl', 'menu-gl-to-metal']:
            command = [str(executable), '-metal' if args.case == 'menu-metal-to-gl' else '-gl',
                       '-windowed', '-w', '800', '-h', '600']
            env['CSE_BOOTSTRAP_ACTIVE_METAL'] = '1' if args.case == 'menu-metal-to-gl' else '0'
            env['CSE_BOOTSTRAP_MENU_RENDERER'] = 'OpenGL' if args.case == 'menu-metal-to-gl' else 'Metal'
        if args.case == 'restart-options':
            command += ['-game', 'valve', '-fullscreen', '-w', '640', '-h', '480', '-name', 'player with spaces',
                        '-extra', 'backslash\\quote"literal', '-literal', 'player "bob" smith']
            env['CSE_BOOTSTRAP_RESTART_OPTIONS'] = '-game cstrike -w 1024 -h 768 -windowed +load "save with spaces"'
        if args.case in ['restart-to-software', 'restart-to-hardware']:
            command += ['-gl' if args.case == 'restart-to-software' else '-soft']
            env['CSE_BOOTSTRAP_RESTART_OPTIONS'] = '-soft' if args.case == 'restart-to-software' else '-gl'
        if args.case in ['restart-metal-to-gl', 'restart-gl-to-metal']:
            command += ['-metal' if args.case == 'restart-metal-to-gl' else '-gl']
            env['CSE_BOOTSTRAP_RESTART_OPTIONS'] = '-gl' if args.case == 'restart-metal-to-gl' else '-metal'
        if args.case == 'metal-cached-software':
            seed_env = dict(env, CSE_BOOTSTRAP_PLAN='restart', CSE_BOOTSTRAP_MENU_RENDERER='Software')
            seed = subprocess.run([str(executable), '-gl'], cwd=cwd, env=seed_env, capture_output=True, timeout=10)
            assert seed.returncode == 0, seed.stderr
            log.unlink()
            command += ['-metal']
        if args.case == 'restart-software':
            command += ['-windowed', '-w', '640', '-h', '480']
        if args.case == 'restart-exec-failure':
            env['CSE_BOOTSTRAP_REMOVE_EXECUTABLE'] = str(executable)
        if args.case == 'explicit-game':
            command += ['-game', 'cstrike']
        if args.case in ['software', 'restart-software']:
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
        elif args.case == 'restart-exec-failure':
            assert result.returncode == 1 and 'Unable to restart the native client' in report['stderr'], report
            assert events.count('run\t') == 1 and 'removed_executable\tyes' in events, events
        else:
            # leaks returns its analysis status; the direct CTest case verifies the process exit status.
            expected_exit = 0
            assert result.returncode == expected_exit, report
            expected_runs = 2 if args.case in ['restart', 'retained', 'restart-options', 'restart-software', 'restart-to-software', 'restart-to-hardware', 'restart-metal-to-gl', 'restart-gl-to-metal', 'menu-software', 'menu-to-software', 'menu-to-hardware', 'menu-metal-to-gl', 'menu-gl-to-metal', 'command-size-only', 'command-renderer-only'] else 1
            if args.case == 'retained':
                assert 'Engine image remains loaded' not in report['stderr'], report
            assert events.count('run\t') == expected_runs, events
            assert events.count('base\t' + str(app)) == expected_runs, events
            assert events.count('cwd\t' + str(app)) == expected_runs, events
            assert events.count('checked_borrowed_filesystem\tyes') == expected_runs, events
            assert events.count('root_file\tyes') == expected_runs, events
            commands = [row.split('\t', 1)[1] for row in events if row.startswith('command\t')]
            if args.case in ['restart-metal-to-gl', 'restart-gl-to-metal', 'metal-cached-software']:
                engines = [Path(row.split('\t', 1)[1]).name for row in events if row.startswith('engine\t')]
                assert all(image == 'hw.dylib' for image in engines), engines
                if args.case != 'metal-cached-software':
                    replacement = '-gl' if args.case == 'restart-metal-to-gl' else '-metal'
                    previous = '-metal' if args.case == 'restart-metal-to-gl' else '-gl'
                    assert replacement in commands[1] and previous not in commands[1], commands
            if args.case == 'command-size-only':
                for value in ['-soft', '-windowed', '-w 1024', '-h 768']:
                    assert value in commands[1], commands
                assert '-w 800' not in commands[1] and '-h 600' not in commands[1], commands
            if args.case == 'command-renderer-only':
                for value in ['-gl', '-fullscreen', '-w 800', '-h 600']:
                    assert value in commands[1], commands
                assert '-soft' not in commands[1] and '-windowed' not in commands[1], commands
            if args.case.startswith('menu-'):
                assert '-windowed' not in commands[1], commands
                dimensions = ['-w 800', '-h 600'] if args.case == 'menu-software' else ['-w 1024', '-h 768']
                for updated in ['-fullscreen', *dimensions]:
                    assert updated in commands[1], commands
                replacement = '-metal' if args.case == 'menu-gl-to-metal' else '-gl' if args.case in ['menu-to-hardware', 'menu-metal-to-gl'] else '-soft'
                assert replacement in commands[1], commands
                engines = [Path(row.split('\t', 1)[1]).name for row in events if row.startswith('engine\t')]
                expected_first = 'hw.dylib' if args.case in ['menu-to-software', 'menu-metal-to-gl', 'menu-gl-to-metal'] else 'swds.dylib'
                expected_final = 'hw.dylib' if args.case in ['menu-to-hardware', 'menu-metal-to-gl', 'menu-gl-to-metal'] else 'swds.dylib'
                assert engines == [expected_first, expected_final], engines
                # A fresh launch has no explicit renderer flag; it must honor the
                # actual video setter's persisted legacy EngineDLL spelling.
                log.unlink()
                env['CSE_BOOTSTRAP_PLAN'] = 'normal'
                fresh = subprocess.run([str(executable)], cwd=cwd, env=env, capture_output=True, timeout=10)
                fresh_events = log.read_text().splitlines()
                assert fresh.returncode == 0, fresh.stderr
                fresh_images = [Path(row.split('\t', 1)[1]).name for row in fresh_events if row.startswith('engine\t')]
                assert fresh_images == [expected_final], fresh_events
                if args.case in ['menu-metal-to-gl', 'menu-gl-to-metal']:
                    previous = '-gl' if args.case == 'menu-gl-to-metal' else '-metal'
                    assert previous not in commands[1], commands
                    saved = 'metal' if args.case == 'menu-gl-to-metal' else 'gl'
                    assert 'saved_renderer\t' + saved in fresh_events, fresh_events
            if args.case in ['software', 'restart-software']:
                engines = [row.split('\t', 1)[1] for row in events if row.startswith('engine\t')]
                assert all(Path(image).name == 'swds.dylib' for image in engines), engines
            if args.case == 'restart-software':
                for preserved in ['-soft', '-windowed', '-w 640', '-h 480']:
                    assert preserved in commands[1], commands
                assert events.count('native_mode\t-soft') == 2, events
            if args.case in ['restart-to-software', 'restart-to-hardware']:
                engines = [Path(row.split('\t', 1)[1]).name for row in events if row.startswith('engine\t')]
                assert engines == (['hw.dylib', 'swds.dylib'] if args.case == 'restart-to-software'
                                   else ['swds.dylib', 'hw.dylib']), engines
                replacement = '-soft' if args.case == 'restart-to-software' else '-gl'
                previous = '-gl' if args.case == 'restart-to-software' else '-soft'
                assert replacement in commands[1] and previous not in commands[1], commands
                assert events.count('native_mode\t' + replacement) == 1, events
            if args.case in ['explicit-game', 'alias']:
                assert '-game cstrike' in commands[0], commands
            if args.case == 'restart-options':
                assert events.count('native_literal\tplayer "bob" smith') == 2, events
                assert '-fullscreen' not in commands[1] and '-w 640' not in commands[1], commands
                for preserved in ['-name "player with spaces"', '-extra backslash\\quote"literal', '-novid',
                                  '-game cstrike', '-w 1024', '-h 768', '-windowed', '+load "save with spaces"']:
                    assert preserved in commands[1], (preserved, commands)
            assert events.count('load\t') == expected_runs, events
            # exec replaces a retained image without running its old process finalizers.
            expected_unloads = 1 if args.case == 'retained' else expected_runs
            assert events.count('unload\t') == expected_unloads, events


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    for key in ['launcher', 'probe', 'engine', 'mismatch', 'nullfs', 'filesystem']:
        parser.add_argument('--' + key, type=Path, required=True)
    parser.add_argument('--case', required=True)
    parser.add_argument('--leaks', action='store_true')
    run(parser.parse_args())
