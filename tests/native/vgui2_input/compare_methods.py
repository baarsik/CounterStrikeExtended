#!/usr/bin/env python3
import json
from pathlib import Path
import subprocess
import sys
result=subprocess.run([sys.argv[1]],capture_output=True,timeout=20)
assert result.returncode==0,(result.returncode,result.stderr)
Path(sys.argv[3]).write_bytes(result.stdout)
actual=[json.loads(line) for line in result.stdout.splitlines()]
expected=[json.loads(line) for line in Path(sys.argv[2]).read_text().splitlines()]
assert len(actual)==len(expected)==6144,(len(actual),len(expected))
for index,(a,b) in enumerate(zip(actual,expected)):
    assert a==b,(index,'actual',a,'expected',b)
print('PASS 6144 actual method rows match Windows ordering/ignore/guard reference; snapshot counts checked in executable')
