"""A failed legacy void shutdown must terminate this owned test process."""
import subprocess
import sys

result = subprocess.run([sys.argv[1], '--join-failure'], stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, timeout=15)
assert result.returncode == 1, (result.returncode, result.stdout)
assert b'ERROR:' not in result.stdout, result.stdout
print('actual-shutdown-held-worker: nonreturning exit=1; no caller continuation')
