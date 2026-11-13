"""Freeze actual loader bodies for the in-memory cross-platform BMP fixture."""
import hashlib
import json
import sys
from pathlib import Path

root = Path(sys.argv[1])
out = Path(sys.argv[2])
out.mkdir(parents=True, exist_ok=True)
bitmap = root / 'src/engine/bitmap_win.cpp'
vgui = root / 'src/engine/vgui_int.cpp'
bitmap_text = bitmap.read_bytes().decode('cp1252')
vgui_text = vgui.read_bytes().decode('cp1252')
body = '#define DIB_HEADER_MARKER 0x4D42\n'
body += bitmap_text[bitmap_text.index('void LoadBMP8('):]
body += '\n' + vgui_text[vgui_text.index('int VGui_LoadBMP('):]
(out / 'loader_bodies.inc').write_text(body, encoding='utf-8')
inputs = [bitmap, vgui, root / 'src/common/legacy_bmp_records.h',
          root / 'src/engine/filesystem_internal.cpp']
(out / 'loader-inputs.json').write_text(json.dumps({
    'scope': 'Actual unchanged loader bodies; injected byte-returning FS_Read and allocation services',
    'inputs': {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest() for path in inputs},
    'bodies_sha256': hashlib.sha256(body.encode()).hexdigest()
}, indent=2) + '\n')
