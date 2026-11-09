"""Mutation controls on private frozen source copies; no game/provider execution."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('source', type=Path)
parser.add_argument('output', type=Path)
options = parser.parse_args()
options.output.mkdir(parents=True, exist_ok=False)
mutations = [
    ('missing-hook', 'GoldSrc-SDK/engine/cdll_int.cpp',
     '    g_engdstAddrs.pfnGetClientMousePositionAndCenter(&x, &y, &centerX, &centerY);', ''),
    ('false-success', 'GoldSrc-SDK/engine/cdll_int.cpp',
     'return cse::TryGetEngineMousePositionAndCenter(*pmainwindow, x, y, centerX, centerY) ? 1 : 0;',
     'cse::TryGetEngineMousePositionAndCenter(*pmainwindow, x, y, centerX, centerY); return 1;'),
    ('partial-write', 'GoldSrc-SDK/engine/cdll_int.cpp',
     '    return cse::TryGetEngineMousePositionAndCenter(*pmainwindow, x, y, centerX, centerY) ? 1 : 0;',
     '    *x = 0;\n    return cse::TryGetEngineMousePositionAndCenter(*pmainwindow, x, y, centerX, centerY) ? 1 : 0;'),
    ('misbound-table', 'GoldSrc-SDK/engine/cdll_int.cpp',
     '    hudGetClientMousePositionAndCenter\n};', '    UnavailableClientMouseQuery\n};'),
    ('unbound-default', 'GoldSrc-SDK/engine/cdll_int.h',
     '    NullClientMousePositionAndCenterDst,', '    nullptr,')]
rows = []
for name, path, before, after in mutations:
    source = options.output / name / 'source'
    shutil.copytree(options.source, source)
    target = source / path
    data = target.read_bytes()
    newline = b'\r\n' if b'\r\n' in data else b'\n'
    original = before.encode().replace(b'\n', newline)
    replacement = after.encode().replace(b'\n', newline)
    assert data.count(original) == 1, (name, data.count(original))
    target.write_bytes(data.replace(original, replacement))
    build = options.output / name / 'build'
    phases = []
    for phase, command in [
        ('configure', ['cmake', '-S', str(source / 'tests/native/client_mouse_callback'), '-B', str(build), '-G', 'Ninja',
         '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_OSX_ARCHITECTURES=arm64', '-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0',
         '-DCSE_SDL_INCLUDE=' + str(source / 'third_party/SDL3/include')]),
        ('build', ['cmake', '--build', str(build), '--parallel', '2']),
        ('test', ['ctest', '--test-dir', str(build), '--output-on-failure'])]:
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)
        (build.parent / (phase + '.log')).write_bytes(result.stdout)
        phases.append({'phase': phase, 'exit': result.returncode})
        if result.returncode:
            break
    detected = len(phases) == 3 and phases[0]['exit'] == phases[1]['exit'] == 0 and phases[2]['exit'] != 0
    rows.append({'name': name, 'mutated_source_sha256': hashlib.sha256(target.read_bytes()).hexdigest(),
                 'phases': phases, 'detected': detected})
    (options.output / 'cases.json').write_text(json.dumps(rows, indent=2) + '\n')
    print(name, 'detected' if detected else 'FAILED', flush=True)
raise SystemExit(int(not all(row['detected'] for row in rows)))
