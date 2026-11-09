"""Exact exit/marker distinguishes termination from an exception escaping the helper."""
import subprocess
import sys
for mode in ['atlas', 'pin']:
    result = subprocess.run([sys.argv[1], sys.argv[2], sys.argv[3], '1', '--fault-' + mode],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=45)
    sys.stdout.buffer.write(result.stdout)
    assert result.returncode == 86, (mode, result.returncode)
    assert b'CONTROLLED standard allocation throws std::bad_alloc' in result.stdout
    assert b'EXPECTED allocation termination before EH host catch' in result.stdout
    assert b'RED allocation exception escaped' not in result.stdout
