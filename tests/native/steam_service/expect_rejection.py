import subprocess
import sys

result = subprocess.run([sys.argv[1]], capture_output=True, text=True)
assert result.returncode == 1, (result.returncode, result.stdout, result.stderr)
assert 'SteamAPI_Shutdown' in result.stderr, result.stderr
assert 'client_cycle=' not in result.stdout, result.stdout
print('Incomplete service provider rejected before static callback registration or client initialization')
