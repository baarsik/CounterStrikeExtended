"""Keep the actual limited-file loader and stream closer in the ownership fixture."""
from pathlib import Path
import sys


def function(text, signature):
    assert text.count(signature) == 1, signature
    start = text.index(signature)
    end = text.index("{", start) + 1
    depth = 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


root = Path(sys.argv[1])
limited = function((root / "common.cpp").read_text(), "byte* COM_LoadFileLimit(")
close = function((root / "snd_mix.cpp").read_text(), 'extern "C" void Wavstream_Close(')
Path(sys.argv[2]).write_text(limited + "\n\n" + close + "\n")
