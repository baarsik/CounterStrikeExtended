from pathlib import Path
import sys


def function(text, name):
    start = text.index('void ' + name + '(')
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


root = Path(sys.argv[1])
save = function((root / 'host_cmd.cpp').read_text(), 'CL_Save')
restore = function((root / 'cl_parse.cpp').read_text(), 'CL_Restore')
Path(sys.argv[2]).write_text(save + '\n' + restore + '\n')
