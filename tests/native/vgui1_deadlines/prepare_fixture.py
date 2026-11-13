"""Copy actual current method bodies; tag only unrelated/cleanup bindings."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

parser = argparse.ArgumentParser()
parser.add_argument('--root', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
root = args.root.resolve()
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)
vgui = root / 'src/utils/vgui/lib-src/vgui'

for view in ['public', 'private']:
    overlay = output / ('headers-' + view)
    overlay.mkdir(exist_ok=True)
    shutil.copy2(root / 'src/utils/vgui/include/VGUI_App.h', overlay / 'VGUI_App.h')
    declaring = root / 'src/utils/vgui/include' if view == 'public' else vgui / 'include'
    for name in ['VGUI_TextEntry.h', 'VGUI_EditPanel.h']:
        shutil.copy2(declaring / name, overlay / name)


def method(text, marker):
    start = text.index(marker)
    body = text.index('{', start)
    depth = 1
    end = body + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return start, end, text[start:end]


rows = []
for name, signal, base, cleanup in [
    ('TextEntry.cpp', 'FooDefaultTextEntrySignal', 'FocusChangeSignal', 'DeleteOwnedTextFocus'),
    ('EditPanel.cpp', 'FooDefaultEditPanelSignal', 'InputSignal', 'DeleteOwnedEditInput'),
]:
    text = (vgui / name).read_text()
    addition = '\n// Fixture-only cleanup of the actual anonymous listener type.\n'
    addition += f'extern "C" void {cleanup}(vgui::{base}* pointer) {{\n'
    addition += f'    auto* actual = static_cast<{signal}*>(pointer);\n'
    addition += f'    actual->{signal}::~{signal}();\n    ::operator delete(actual);\n}}\n'
    (output / name).write_text(text + addition)
    rows.append({'source': str(vgui / name), 'sha256': hashlib.sha256((vgui / name).read_bytes()).hexdigest(),
                 'change': 'append cleanup only; all actual normalized method source is exact prefix'})
text = (vgui / 'Panel.cpp').read_text()
start, end, original = method(text, 'Panel* Panel::createPropertyPanel()')
replacement = 'Panel* Panel::createPropertyPanel()\n{\n    std::fputs("unqualified property UI path\\n", stderr);\n    std::abort();\n}'
(output / 'Panel-controlled-unreachable-property.cpp').write_text(
    '#include <cstdio>\n#include <cstdlib>\n' + text[:start] + replacement + text[end:])
rows.append({'source': str(vgui / 'Panel.cpp'), 'sha256': hashlib.sha256((vgui / 'Panel.cpp').read_bytes()).hexdigest(),
             'change': 'replace only unrelated/unexecuted property factory by fatal controlled binding',
             'original_method_sha256': hashlib.sha256(original.encode()).hexdigest()})
text = (vgui / 'posix/App.cpp').read_text()
start, end, native = method(text, 'long App::getTimeMillis()')
_, _, windows = method((vgui / 'win32/App.cpp').read_text(), 'long App::getTimeMillis()')
replacement = '#if defined(_WIN32)\n' + windows + '\n#else\n' + native + '\n#endif'
(output / 'App-controlled-clock.cpp').write_text(
    '#if defined(_WIN32)\nunsigned long GetTickCount(); // Controlled DWORD-shaped clock, no host API.\n#endif\n'
    + text[:start] + replacement + text[end:])
rows.append({'source': str(vgui / 'posix/App.cpp'), 'sha256': hashlib.sha256((vgui / 'posix/App.cpp').read_bytes()).hexdigest(),
             'change': 'Windows only: exact retained clock body and controlled GetTickCount; native producer unchanged',
             'windows_sha256': hashlib.sha256((vgui / 'win32/App.cpp').read_bytes()).hexdigest()})
(output / 'controlled-bindings.json').write_text(json.dumps(rows, indent=2) + '\n')
