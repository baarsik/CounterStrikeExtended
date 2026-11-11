import pathlib
import sys

source = pathlib.Path(sys.argv[1]).read_text()
start = source.index('void InstallBotControl()')
body = source.index('{', start)
depth = 1
end = body + 1
while depth:
    depth += (source[end] == '{') - (source[end] == '}')
    end += 1
pathlib.Path(sys.argv[2]).write_text(source[start:end] + '\n')
