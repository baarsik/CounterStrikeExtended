"""Reject mixed/missing native cohorts through the real launcher before engine load."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

runtime = Path(sys.argv[1]).resolve(strict=True)
message = "Native engine/VGUI images do not match this launcher's built cohort."
cases = [('hw.dylib', '-gl'), ('vgui.dylib', '-gl')]
if (runtime / 'swds.dylib').exists():
    cases += [('swds.dylib', '-soft'), ('vgui.dylib', '-soft')]
for rejected, mode in cases:
    for missing in (False, True):
        with tempfile.TemporaryDirectory(prefix='cse-cohort-') as directory:
            root = Path(directory)
            for image in runtime.glob('*.dylib'):
                if image.name != rejected:
                    # Dependencies stay read-only in the finished build; the
                    # launcher and rejected member belong to this temporary root.
                    if image.name in ('hw.dylib', 'swds.dylib', 'vgui.dylib'):
                        shutil.copy2(image, root / image.name)
                    else:
                        (root / image.name).symlink_to(image)
            shutil.copy2(runtime / 'hl', root / 'hl')
            if not missing:
                (root / rejected).write_bytes(b'mixed native image must never reach dlopen\n')
            result = subprocess.run([str(root / 'hl'), mode, '-game', 'valve', '-nobreakpad'],
                cwd=root, capture_output=True, text=True, timeout=15)
            if result.returncode != 1 or message not in result.stderr:
                raise RuntimeError(f'{rejected} missing={missing}: {result.returncode}\n'
                    f'{result.stdout}\n{result.stderr}')
            if 'Console initialized' in result.stdout or 'SteamAPI_Init' in result.stderr:
                raise RuntimeError('engine/provider callbacks ran before native cohort rejection')
print(f'Actual launcher rejected {len(cases) * 2} missing/mixed engine/VGUI cohorts before engine callbacks')
