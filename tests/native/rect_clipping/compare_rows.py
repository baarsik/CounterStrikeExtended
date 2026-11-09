#!/usr/bin/env python3
import json
from pathlib import Path
import subprocess
import sys
result=subprocess.run([sys.argv[1]],capture_output=True,timeout=15)
assert result.returncode==0,(result.returncode,result.stderr)
output=Path(sys.argv[3]);output.write_bytes(result.stdout)
expected=[json.loads(line) for line in Path(sys.argv[2]).read_text().splitlines()]
actual=[json.loads(line) for line in result.stdout.splitlines()]
assert len(actual)==len(expected),(len(actual),len(expected))
for index,(a,b) in enumerate(zip(actual,expected)):
    assert a==b,(index,'actual',a,'expected',b)
print('PASS',len(actual),'actual imported body rows match real Windows reference exactly')
