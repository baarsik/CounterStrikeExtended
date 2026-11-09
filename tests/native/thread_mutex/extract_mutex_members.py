"""Keep complete production method bodies; model storage with explicit LLP64 widths.

The portable fixture does not include a Windows SDK, replace C++ keywords, or
claim the original Windows class/ABI was compiled. Windows uses the whole header.
"""
from pathlib import Path
import re
import sys

source = Path(sys.argv[1]).read_text(encoding='cp1252')
classes = list(re.finditer(r'^class TIER0_API CThreadFastMutex\s*\{', source, re.M))
if len(classes) != 1:
    raise SystemExit('Expected one production CThreadFastMutex class')
start = classes[0].end()
fields = list(re.finditer(r'^\s*volatile unsigned long\s+m_ownerID;\s*\n\s*int\s+m_depth;', source[start:], re.M))
if len(fields) != 1:
    raise SystemExit('Expected the unchanged unsigned long owner and int depth fields')
members = source[start:start + fields[0].start()]
if members.count('bool TryLock() volatile') != 1 or members.count('void Unlock() volatile') != 1:
    raise SystemExit('Production mutex method boundaries changed')
Path(sys.argv[2]).write_text(members + '\n', encoding='utf-8')
