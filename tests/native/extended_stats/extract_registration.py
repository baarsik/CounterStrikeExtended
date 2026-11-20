from pathlib import Path
import sys
text = (Path(sys.argv[1]) / 'src/amxx/csextended/stats/stats.cpp').read_text()
name = text.index('RegisterMessages()')
begin = text.rfind('\n', 0, name) + 1
end = text.index('{', name) + 1
depth = 1
while depth:
    depth += (text[end] == '{') - (text[end] == '}')
    end += 1
result = text[begin:end]
name = text.index('ResolveLegacyMessages()')
begin = text.rfind('\n', 0, name) + 1
end = text.index('{', name) + 1
depth = 1
while depth:
    depth += (text[end] == '{') - (text[end] == '}')
    end += 1
Path(sys.argv[2]).write_text(text[begin:end] + '\n' + result)
