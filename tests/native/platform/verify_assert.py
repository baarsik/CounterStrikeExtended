"""Assertions must remain active even when NDEBUG is supplied by a caller."""
import os
import signal
import subprocess
import sys

executable = sys.argv[1]
subprocess.run([executable], check=True)
if os.name != 'nt':
    for operation, expression in [('invalid-memory', b'IsIdxValid(i)'), ('invalid-vector', b'IsValidIndex(i)'),
                                  ('invalid-tree', b'm_LessFunc')]:
        result = subprocess.run([executable, operation], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if result.returncode != -signal.SIGABRT or expression not in result.stderr:
            raise SystemExit(f'{operation}: expected SIGABRT and assertion text; got {result.returncode}, {result.stderr!r}')
