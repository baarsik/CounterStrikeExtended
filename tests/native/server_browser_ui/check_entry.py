import subprocess
import sys
result = subprocess.run(sys.argv[1:], capture_output=True, text=True)
print(result.stdout, end='')
print(result.stderr, end='', file=sys.stderr)
case = sys.argv[-1]
if case in ('invalid-entry', 'exception', 'reentry'):
    assert result.returncode == 86, 'invalid/unknown native ownership returned normally'
    assert 'fatal-invariant-observed' in result.stdout
    assert 'audio-guard-destroyed' not in result.stdout, 'fatal invariant crossed audio cleanup'
    assert 'caller-image-unload-authorized' not in result.stdout, 'fatal invariant authorized launcher unload'
    if case == 'invalid-entry':
        assert 'audio-guard-created' not in result.stdout, 'invalid entry created cleanup guard'
else:
    assert result.returncode == 0
    assert 'listen-entered-pending-0' in result.stdout, 'new listen started before previous ownership retired'
    assert result.stdout.count('audio-guard-destroyed') == 1
    assert result.stdout.count('caller-image-unload-authorized') == 1
    if case == 'pending':
        assert result.stdout.count('retirement-yield') == 2
