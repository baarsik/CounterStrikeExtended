import os
import pty
import signal
import subprocess
import sys
import termios

probe, case = sys.argv[1:]
if case == 'pty_restore_signals':
    for stop in (signal.SIGINT, signal.SIGTERM):
        master, slave = pty.openpty()
        before = termios.tcgetattr(slave)
        child = subprocess.Popen([probe], stdin=slave, stdout=slave, stderr=subprocess.PIPE)
        assert child.stderr.readline() == b'READY\n'
        assert not (termios.tcgetattr(slave)[3] & termios.ICANON)
        os.write(master, b'abc\x1b[D\x7fZ\n')
        assert child.stderr.readline() == b'LINE:aZc\n'
        child.send_signal(stop)
        _, errors = child.communicate(timeout=3)
        assert child.returncode == 0 and b'CLOSED' in errors
        os.set_blocking(master, False)
        assert b'SHUTDOWN_OUTPUT' in os.read(master, 4096)
        restored = termios.tcgetattr(slave)
        # Darwin sets PENDIN when restoring canonical input so queued bytes are
        # reprocessed; this kernel bookkeeping bit is not a changed user mode.
        restored[3] &= ~getattr(termios, 'PENDIN', 0)
        before[3] &= ~getattr(termios, 'PENDIN', 0)
        assert restored == before
        os.close(master)
        os.close(slave)
elif case == 'null_input':
    result = subprocess.run([probe], stdin=subprocess.DEVNULL, capture_output=True, timeout=3)
    assert result.returncode == 0 and b'EOF_FRAMES:5' in result.stderr, result.stderr
elif case == 'closed_descriptors':
    def close_stdio():
        os.close(0)
        os.close(1)
    result = subprocess.run([probe], preexec_fn=close_stdio, stderr=subprocess.PIPE, timeout=3)
    assert result.returncode == 0 and b'EOF_FRAMES:5' in result.stderr
else:
    arguments = [probe, 'repeat'] if case == 'repeated_open' else [probe]
    result = subprocess.run(arguments, input=b'one\nfinal', stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, timeout=3)
    assert result.returncode == 0, result.stderr
    assert result.stderr.count(b'LINE:final\n') == 1, result.stderr
    assert b'LINE:one\n' in result.stderr and b'EOF_FRAMES:5\n' in result.stderr
