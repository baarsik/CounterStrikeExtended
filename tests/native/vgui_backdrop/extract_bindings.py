"""Compile the real shared and CS controls lifecycle around a minimal interface factory."""
from pathlib import Path
import sys

root = Path(sys.argv[1])
output = Path(sys.argv[2])
output.mkdir(parents=True, exist_ok=True)


def function(source, signature):
    start = source.index(signature)
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


for name, path in [('shared', 'src/vgui2/controls/controls.cpp'),
                   ('cs', 'src/cstrike/vgui2/controls/controls.cpp')]:
    source = (root / path).read_text(encoding='cp1252')
    signatures = ['static void* InitializeInterface(', 'bool VGuiControls_Init(', 'void VGuiControls_Shutdown()']
    (output / f'{name}_bindings.inc').write_text('\n'.join(function(source, signature) for signature in signatures))
