"""Run a legal-ownership actual-module scenario through Apple's leak checker."""

import os
from pathlib import Path
import re
import subprocess
import sys


if __name__ == '__main__':
    leaks, driver, module, directory, scenario, report = sys.argv[1:]
    environment = dict(os.environ, MallocStackLogging='1')
    result = subprocess.run([leaks, '--atExit', '--', driver, module, directory, scenario],
                            env=environment, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    output = Path(report)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(result.stdout)
    summary = re.findall(r'Process \d+: \d+ leaks for \d+ total leaked bytes\.', result.stdout)
    print(summary[-1] if summary else 'No leak-check summary; see ' + str(output))
    if (result.returncode or '0 leaks for 0 total leaked bytes.' not in result.stdout
            or f'PASS: filesystem {scenario}' not in result.stdout or 'FAIL:' in result.stdout):
        raise SystemExit(1)
