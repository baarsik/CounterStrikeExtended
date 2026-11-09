#!/usr/bin/env python3
import resource
import signal
import subprocess
import sys
resource.setrlimit(resource.RLIMIT_CORE,(0,0))
for mode in ['missing-snapshot','short-snapshot','unsupported-set-cursor','unsupported-get-cursor']:
    result=subprocess.run([sys.argv[1],mode],capture_output=True,text=True,timeout=10)
    assert result.returncode==-signal.SIGABRT,(mode,result.returncode,result.stdout,result.stderr)
    assert 'VGUI2 native input:' in result.stderr,(mode,result.stderr)
    assert ('required scancodes' if mode.endswith('snapshot') else 'requires native provider cursor functions') in result.stderr
    print('PASS',mode,'diagnostic + nonreturning SIGABRT')
