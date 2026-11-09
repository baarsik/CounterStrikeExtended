from pathlib import Path
import subprocess
import sys

result = subprocess.run([sys.argv[1]], capture_output=True, timeout=30)
Path(sys.argv[2]).write_bytes(result.stdout)
assert result.returncode == 0, (result.returncode, result.stderr)
subprocess.run([sys.executable, str(Path(__file__).with_name('check_rows.py')), sys.argv[2]], check=True)
