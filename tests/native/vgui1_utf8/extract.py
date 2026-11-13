"""Extract actual production methods for a typed CPU/font receiver."""
import pathlib
import re
import sys

root = pathlib.Path(sys.argv[1])
out = pathlib.Path(sys.argv[2])
vgui = root / 'src/utils/vgui/lib-src/vgui'
methods = []
image_source = (vgui / 'TextImage.cpp').read_text()
helper = re.search(r'^namespace \{\nstruct TextExtent', image_source, re.M)
if helper:
    first = image_source.index('{', helper.start())
    depth = 1
    end = first + 1
    while depth:
        depth += (image_source[end] == '{') - (image_source[end] == '}')
        end += 1
    methods.append(image_source[helper.start():end])
for file, names in [('Font.cpp', ['Font::getTextSize']),
                    ('TextImage.cpp', ['TextImage::getTextSizeWrapped', 'TextImage::paint'])]:
    source = (vgui / file).read_text()
    for name in names:
        match = re.search(r'^void ' + re.escape(name) + r'\([^\n]*\)\s*\{', source, re.M)
        assert match, name
        first = source.index('{', match.start())
        depth = 1
        end = first + 1
        while depth:
            depth += (source[end] == '{') - (source[end] == '}')
            end += 1
        methods.append(source[match.start():end])
out.mkdir(parents=True, exist_ok=True)
(out / 'methods.inc').write_text('\n\n'.join(methods) + '\n')
windows = (vgui / 'win32/Font.cpp').read_bytes().decode('cp1252')
windows_methods = []
for pattern in [r'    ABC CharacterMetrics\(int character\)', r'\tvirtual void getCharRGBA\(']:
    match = re.search(pattern, windows)
    assert match, pattern
    first = windows.index('{', match.start())
    depth = 1
    end = first + 1
    while depth:
        depth += (windows[end] == '{') - (windows[end] == '}')
        end += 1
    windows_methods.append(windows[match.start():end].replace(' override', ''))
(out / 'windows-methods.inc').write_text('\n\n'.join(windows_methods) + '\n')
