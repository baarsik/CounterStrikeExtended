"""Require a completed actual particle fixture and zero leaks, without suppression."""

import os
from pathlib import Path
import re
import subprocess
import sys


if __name__ == '__main__':
    report, marker, leaks, *command = sys.argv[1:]
    result = subprocess.run([leaks, '--atExit', '--', *command], env=dict(os.environ, MallocStackLogging='1'),
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    output = Path(report)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(result.stdout)
    summaries = re.findall(r'Process \d+: \d+ leaks for \d+ total leaked bytes\.', result.stdout)
    print(summaries[-1] if summaries else 'No leak summary: ' + report)
    if (result.returncode or marker not in result.stdout or 'failure:' in result.stdout
            or '0 leaks for 0 total leaked bytes.' not in result.stdout):
        raise SystemExit(1)
