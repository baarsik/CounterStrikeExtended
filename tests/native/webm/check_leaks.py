"""Require actual decoder fixture completion and zero Apple leak-check bytes."""

import os
from pathlib import Path
import subprocess
import sys


if __name__ == '__main__':
    report, leaks, *command = sys.argv[1:]
    result = subprocess.run([leaks, '--atExit', '--', *command], env=dict(os.environ, MallocStackLogging='1'),
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    path = Path(report)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(result.stdout)
    if (result.returncode or 'Native WebM fixture passed' not in result.stdout
            or '0 leaks for 0 total leaked bytes.' not in result.stdout or 'failure:' in result.stdout):
        raise SystemExit('WebM ownership fixture failed: ' + report)
    print('Native WebM fixture completed with zero leaked bytes.')
