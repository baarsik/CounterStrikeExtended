#!/usr/bin/env python3
"""Fresh actual VGUI1 provider/face/owner compilation, using read-only Ninja recipes."""
from pathlib import Path
import argparse
import hashlib
import json
import shlex
import subprocess
from concurrent.futures import ThreadPoolExecutor
p = argparse.ArgumentParser()
p.add_argument('--provider-build', type=Path, default=Path('build/vgui1-utf8'))
p.add_argument('--evidence', type=Path, required=True)
p.add_argument('--bitmap', type=Path, required=True, nargs='+')
p.add_argument('--baseline', action='store_true')
p.add_argument('--driver', type=Path)
p.add_argument('--provider-only', action='store_true')
a = p.parse_args()
r = Path(__file__).resolve().parents[3]
out = a.evidence.resolve(); out.mkdir(parents=True, exist_ok=True)
build = a.provider_build.resolve()
raw = subprocess.check_output(['ninja', '-C', str(build), '-t', 'commands', 'native_owner_tests'], text=True).splitlines()
commands = [shlex.split(line) for line in raw]
link = commands[-1][2:-2]
compiled = []
for command in commands[:-1]:
    if '-c' not in command:
        continue
    filename = Path(command[-1])
    if filename.name == 'MacFontRegistration.cpp':
        continue
    fresh = command[:]
    old_object = fresh[fresh.index('-o') + 1]
    target = out / (str(len(compiled)) + '-' + filename.stem + '.o')
    if filename.name == 'owner_tests.cpp':
        fresh[-1] = str(a.driver.resolve() if a.driver else r / 'tests/native/vgui1_cjk/cjk.cpp')
    if a.baseline and filename.name == 'VGui1MacFontProvider.cpp':
        old = out / 'baseline-provider.cpp'
        old.write_bytes(subprocess.check_output(['git', 'show', 'HEAD:src/utils/vgui/lib-src/vgui/posix/native/VGui1MacFontProvider.cpp'], cwd=r))
        fresh[-1] = str(old)
    for flag, value in [('-o', str(target)), ('-MF', str(target) + '.d'), ('-MT', str(target))]:
        if flag in fresh: fresh[fresh.index(flag) + 1] = value
    fresh[1:1] = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-I' + str(r / 'src/vgui2/vgui_surfacelib/native')]
    fresh.insert(-2, "-std=c++17")
    compiled.append((fresh, old_object, target, str(filename)))

def execute(item):
    result = subprocess.run(item[0], cwd=build, text=True, capture_output=True)
    (out / (item[2].stem + '.log')).write_text(result.stdout + result.stderr)
    result.check_returncode()
with ThreadPoolExecutor(max_workers=4) as pool:
    list(pool.map(execute, compiled))
# Replace all rebuilt static archives and original main/owner objects with fresh objects.
link = [x for x in link if not x.endswith(('.o', 'libnative_provider.a', 'libnative_face.a', 'libprovider_discovery.a'))]
link[link.index('-o') + 1] = str(out / 'cjk')
selected = [item for item in compiled if not a.provider_only or
    '/posix/native/' in item[3] or Path(item[3]).name in ('MacFontDiscovery.cpp', 'owner_tests.cpp')]
link[1:1] = ['-fsanitize=address,undefined'] + [str(item[2]) for item in selected]
(out / 'commands.json').write_text(json.dumps({'compile': [x[0] for x in compiled], 'link': link}, indent=2))
subprocess.run(link, cwd=build, check=True)
result = subprocess.run([str(out / 'cjk'), *[str(path.resolve()) for path in a.bitmap]], text=True, capture_output=True)
(out / 'run.log').write_text(result.stdout + result.stderr)
(out / 'sources.json').write_text(json.dumps({x[0][-1]: hashlib.sha256(Path(x[0][-1]).read_bytes()).hexdigest() for x in compiled}, indent=2))
print('exit', result.returncode, (result.stdout + result.stderr)[-2500:])
if a.baseline:
    assert result.returncode != 0, 'Expected missing CJK glyph RED'
else:
    result.check_returncode()
