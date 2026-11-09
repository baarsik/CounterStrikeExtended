"""Only unexpected use of an old API without a usable face must fail closed."""
import signal
import subprocess
import sys

for mode, message in (
        ('pending', 'font resources are not bound'),
        ('unavailable', 'owned model unavailable'),
        ('released', 'font resources are not bound'),
        ('glyph', 'outside the legacy byte domain'),
        ('canvas', 'invalid legacy raster canvas')):
    result = subprocess.run([sys.argv[1], mode, sys.argv[2]], capture_output=True, text=True, timeout=5)
    assert result.returncode == -signal.SIGABRT, (mode, result.returncode, result.stderr)
    assert 'legacy API used without a ready face' in result.stderr and message in result.stderr, result.stderr
    print(f'PASS fail-closed unexpected old-API {mode}: SIGABRT with specific diagnostic')
