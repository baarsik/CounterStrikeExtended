"""Load every client-owned module eagerly in a fresh process, using real imports."""
import ctypes
from pathlib import Path
import subprocess
import sys

IMAGES = (
    'vgui.dylib', 'tier0.dylib', 'vgui2.dylib', 'filesystem_stdio.dylib',
    'hw.dylib', 'Core.dylib', 'DemoPlayer.dylib',
    'cstrike/cl_dlls/client.dylib', 'cstrike/dlls/mp.dylib',
    'valve/cl_dlls/client.dylib', 'valve/dlls/hl.dylib',
    'valve/cl_dlls/GameUI.dylib', 'valve/cl_dlls/particleman.dylib',
)

if len(sys.argv) == 3:
    library = ctypes.CDLL(sys.argv[2], mode=ctypes.RTLD_LOCAL)
else:
    runtime = Path(sys.argv[1]).resolve(strict=True)
    for relative in IMAGES:
        image = runtime / relative
        architecture = subprocess.check_output(['lipo', '-archs', image], text=True).strip()
        if architecture != 'arm64':
            raise RuntimeError(f'{relative}: expected arm64, got {architecture}')
        subprocess.run([sys.executable, __file__, '--image', str(image)], cwd=runtime, check=True)
    launcher = runtime / 'hl'
    if subprocess.check_output(['lipo', '-archs', launcher], text=True).strip() != 'arm64':
        raise RuntimeError('launcher is not ARM64')
    print(f'Loaded {len(IMAGES)} actual ARM64 client modules; launcher architecture checked')
