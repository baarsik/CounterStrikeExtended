from pathlib import Path
import sys
source = (Path(sys.argv[1]) / 'src/amxx/csextended/stats/legacy_messages.cpp').read_text()
Path(sys.argv[2]).write_text('\n'.join(line for line in source.splitlines() if not line.startswith('#include')))
