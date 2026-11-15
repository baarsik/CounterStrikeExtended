from pathlib import Path
import sys

root = Path(sys.argv[1])
output = Path(sys.argv[2])
output.mkdir(parents=True, exist_ok=True)


def function(path, signature):
    source = (root / path).read_text(encoding='cp1252')
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


helper = (root / 'src/utils/vgui/lib-src/vgui/SmokedGlass.h').read_text()
helper = '\n'.join(line for line in helper.splitlines() if not line.startswith(('#include', '#pragma')))
parts = [helper,
    function('src/utils/vgui/lib-src/vgui/Panel.cpp', 'void Panel::paintBackground()'),
    function('src/cl_dll/vgui_ScorePanel.cpp', 'void CLabelHeader::paintBackground()')]
(output / 'legacy_style.inc').write_text('\n\n'.join(parts))
