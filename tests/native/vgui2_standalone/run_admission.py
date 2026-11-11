import hashlib
from pathlib import Path
import subprocess
import sys
executable, runtime, wrong, contract = sys.argv[1:]
vgui = Path(runtime) / 'vgui2.dylib'
filesystem = Path(runtime) / 'filesystem_stdio.dylib'
hash_file = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
raise SystemExit(subprocess.run([executable, str(vgui), str(filesystem), hash_file(vgui), hash_file(filesystem), wrong, contract]).returncode)
