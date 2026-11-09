from pathlib import Path
import sys

source = Path(sys.argv[1]).read_text(encoding='latin-1')
start = source.index('void Panel::InternalKeyTyped( int unichar )')
body = source.index('{', start)
depth = 0
for end in range(body, len(source)):
    if source[end] == '{':
        depth += 1
    elif source[end] == '}':
        depth -= 1
        if depth == 0:
            Path(sys.argv[2]).write_text(source[start:end + 1] + '\n')
            break
else:
    raise SystemExit('Incomplete actual key-typed method')
