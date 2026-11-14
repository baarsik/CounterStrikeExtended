from pathlib import Path
import sys
text = (Path(sys.argv[1]) / 'src/amxx/cse_extstats/module.cpp').read_text()
name = text.index('RegisterMessages()')
begin = text.rfind('\n', 0, name) + 1
end = text.index('{', name) + 1
depth = 1
while depth:
    depth += (text[end] == '{') - (text[end] == '}')
    end += 1
Path(sys.argv[2]).write_text(text[begin:end])
