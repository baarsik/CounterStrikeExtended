import os
import pty
import termios
import pathlib
import subprocess
import shutil
import sys
import tempfile
binary, case = sys.argv[1:]
with tempfile.TemporaryDirectory() as directory:
    events = pathlib.Path(directory) / 'events'
    env = dict(os.environ, CSE_TEST_EVENTS=str(events))
    args = [binary]
    if case == 'excess_arguments_before_effects':
        args += ['excess_arguments']
    elif case == 'wrong_actual_view':
        env['CSE_TEST_BAD_VIEW'] = '1'
    elif case in ('retirement_refuses_unload', 'retirement_restores_pty'):
        env['CSE_TEST_RETIRE_FAIL'] = '1'
    elif case == 'partial_init_retry':
        args += ['retry']
    elif case == 'preconstructor_cohort_reject':
        root = pathlib.Path(binary).parent
        for name in ['swds.dylib', 'vgui.dylib']:
            shutil.copyfile(root / name, pathlib.Path(directory) / name)
        with (pathlib.Path(directory) / 'swds.dylib').open('ab') as output:
            output.write(b'mismatch')
        args += ['module', directory]
    elif case == 'cohort_reject_before_callbacks':
        env['CSE_TEST_BAD_ROLE'] = '1'
    if case == 'retirement_restores_pty':
        master, slave = pty.openpty()
        before = termios.tcgetattr(slave)
        try:
            run = subprocess.run(args, stdin=slave, stdout=slave, stderr=subprocess.PIPE, env=env, timeout=4)
            restored = termios.tcgetattr(slave)
            restored[3] &= ~getattr(termios, 'PENDIN', 0)
            before[3] &= ~getattr(termios, 'PENDIN', 0)
            assert restored == before, (before, restored, run.stderr)
        finally:
            # Even the RED regression must not leak mode changes to its owned PTY.
            termios.tcsetattr(slave, termios.TCSANOW, before)
            os.close(master)
            os.close(slave)
    else:
        run = subprocess.run(args, input=b'status\nfinal', capture_output=True, env=env, timeout=4)
    calls = events.read_text().splitlines() if events.exists() else []
    if case == 'excess_arguments_before_effects':
        assert run.returncode == 0 and calls == [], (run.stderr, calls)
    elif case in ('retirement_refuses_unload', 'retirement_restores_pty'):
        assert run.returncode != 0 and calls.count('shutdown') == 1, (run.stderr, calls)
        assert 'unmount' not in calls and 'unload' not in calls, calls
    elif case == 'wrong_actual_view':
        assert run.returncode != 0 and 'init' not in calls and 'shutdown' not in calls, calls
    elif case in ('cohort_reject_before_callbacks', 'preconstructor_cohort_reject', 'missing_retirement_export'):
        assert run.returncode != 0
        if case == 'preconstructor_cohort_reject':
            assert calls == [], calls
        assert not any(c in calls for c in ['factory', 'init', 'shutdown', 'frame']), calls
    else:
        assert run.returncode == 0, (run.stderr, calls)
        count = 2 if case == 'partial_init_retry' else 1
        for name in ['mount', 'init', 'shutdown', 'unmount', 'retire', 'unload']:
            assert calls.count(name) == count, (name, calls)
        assert calls.index('shutdown') < calls.index('retire') < calls.index('unmount') < calls.index('unload'), calls
        assert calls.count('frame') == 8, calls
        assert 'final' in calls, calls
        assert b'shutdown output' in run.stdout
