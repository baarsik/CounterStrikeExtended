from pathlib import Path
import sys

text = Path(sys.argv[1]).read_text()
parts = []
for signature in ['static void InitializeEntityHashTable()', 'int GetEntityAPI(', 'int GetEntityAPI2(']:
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    parts.append(text[start:end])
Path(sys.argv[2]).write_text('\n'.join(parts) + '\n')
