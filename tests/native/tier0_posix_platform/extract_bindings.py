"""Use the exact production VCR prefix declarations for the explicitly controlled hook binding."""
from pathlib import Path
import sys

data = Path(sys.argv[1]).read_bytes()
start = data.index(b'enum VCRMode')
last = b'extern VCR_t*\tg_pVCR;'
end = data.index(last, start) + len(last)
Path(sys.argv[2]).write_bytes(b'#pragma once\nextern "C" {\n' + data[start:end] + b'\n}\n')
