"""Use a private pre-epoch file to compare both actual declarations against the actual provider."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix="cse-filesystem-abi-") as directory:
    sample = Path(directory) / "time.dat"
    sample.write_bytes(b"controlled timestamp\n")
    os.utime(sample, (-1, -1))
    result = subprocess.run([sys.argv[1], sys.argv[2], directory], check=False)
    sys.exit(result.returncode)
