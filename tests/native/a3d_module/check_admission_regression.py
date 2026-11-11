"""An early real call must fail the same zero-call gate, with complete cleanup."""
import os
import re
import subprocess
import sys

caller, module, case = sys.argv[1:]
assert os.environ.get('SDL_AUDIO_DRIVER') == 'dummy'
result = subprocess.run([caller, module, case], text=True, stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, timeout=25)
print(result.stdout, end='')
assert result.returncode == 71, f'expected gate failure 71, got {result.returncode}'
expected_counts = ('factory=1 virtual=1 before_admission=2' if case.endswith('early_factory')
                   else 'factory=0 virtual=2 before_admission=2')
assert f'{expected_counts} zero_call_gate=FAIL' in result.stdout
assert 'admission fixture clean: owners=0 audio=baseline unload=S_OK image=absent' in result.stdout
if case.endswith('early_virtual'):
    assert re.search(r'positive observed factory=2 virtual=[1-9][0-9]* before_admission=0 anchor=1', result.stdout)
print(f'{case}: actual early dispatch detected; child gate failed and cleaned up')
