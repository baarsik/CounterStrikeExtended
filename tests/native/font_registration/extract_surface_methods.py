"""Compile the real AddCustomFontFile bodies without starting either full UI."""

from pathlib import Path
import sys


def extract(source, signature):
    start = source.index(signature)
    body = source.index('{', start)
    depth = 1
    end = body + 1
    while depth:
        if source[end] == '{':
            depth += 1
        elif source[end] == '}':
            depth -= 1
        end += 1
    return source[start:end]


if __name__ == '__main__':
    root = Path(sys.argv[1])
    methods = [
        ('GoldSrc-SDK/engine/vgui2/BaseUISurface.cpp', 'bool BaseUISurface::AddCustomFontFile('),
        ('GoldSrc-SDK/vgui2/src/Surface.cpp', 'bool CWin32Surface::AddCustomFontFile('),
    ]
    output = '\n\n'.join(extract((root / path).read_text(encoding='cp1252'), signature)
                         for path, signature in methods)
    Path(sys.argv[2]).write_text(output + '\n')
