"""Preserve retained public game exports while isolating private Mach-O C++ state."""
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools'))
from macho_game_exports import check_exports

runtime = Path(sys.argv[1]).resolve(strict=True)
images = (
    ('cstrike/cl_dlls/client.dylib', 'cmake/exports/win32/cstrike_cdll.def'),
    ('cstrike/dlls/mp.dylib', 'cmake/exports/win32/cstrikedll.def'),
    ('valve/cl_dlls/client.dylib', 'cmake/exports/win32/hl_cdll.def'),
    ('valve/dlls/hl.dylib', 'src/dlls/hl.def'),
)
for image, manifest in images:
    names = []
    for line in (root / manifest).read_text(encoding='latin1').splitlines():
        line = line.strip()
        if re.match(r'^SECTIONS\b', line, re.I):
            break
        if not line or line.startswith(';') or re.match(r'^(LIBRARY|EXPORTS)\b', line, re.I):
            continue
        names.append(line.split()[0].split('=', 1)[0])
    if '/cl_dlls/' in image:
        # Win32's HL client .def also reexports implementation classes from its
        # static VGUI1 dependency. Native consumers bind that implementation in
        # the shared vgui.dylib; preserve the client's actual game/SDK entries.
        names = [name for name in names if '@vgui@@' not in name]
    symbols = subprocess.check_output(['nm', '-gjU', str(runtime / image)], text=True).splitlines()
    demangled = subprocess.check_output(['c++filt'], input='\n'.join(symbols), text=True).splitlines()
    check_exports(names, symbols, demangled)
    if any('GetClassPtr' in name or name in ('__ZTV4CAWP', '__ZTI4CAWP') for name in symbols):
        raise RuntimeError(f'{image}: private gameplay template/type metadata is public')
    print(f'{image}: retained exports present; private gameplay identities isolated')
