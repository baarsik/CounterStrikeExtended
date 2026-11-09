"""Freeze inputs before building the Apple adapter with fixture-only services."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command, log, environment=None, timeout=90):
    with log.open('wb') as output:
        result = subprocess.run(command, stdout=output, stderr=subprocess.STDOUT, env=environment, timeout=timeout)
    return {'command': [str(x) for x in command], 'exit': result.returncode, 'log': str(log)}


parser = argparse.ArgumentParser()
parser.add_argument('--root', required=True, type=Path)
parser.add_argument('--art', required=True, type=Path)
parser.add_argument('--build-base', required=True, type=Path)
parser.add_argument('--dependency-build', required=True, type=Path)
args = parser.parse_args()
root = args.root.resolve()
art = args.art.resolve()
art.mkdir(parents=True)
source = art / 'source'
source.mkdir()
deps = subprocess.check_output(['ninja', '-C', str(args.dependency_build), '-t', 'deps'], text=True)
inputs = set()
for line in deps.splitlines():
    name = Path(line.strip())
    if name.is_file() and root in name.parents:
        inputs.add(name.relative_to(root))
inputs.update(x.relative_to(root) for x in (root / 'tests/native/vgui_font_services/component').iterdir()
              if x.is_file())
for name in sorted(inputs):
    target = source / name
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(root / name, target)
hashes = {str(name): sha(source / name) for name in sorted(inputs)}
(art / 'source-hashes.json').write_text(json.dumps(hashes, indent=2) + '\n')
with tarfile.open(art / 'source.tar.gz', 'w:gz') as archive:
    for name in sorted(inputs):
        archive.add(source / name, arcname=str(name))
profiles = [('release', 'Release', False, False), ('debug', 'Debug', False, False),
            ('sanitizers', 'Debug', True, False), ('fast-release', 'Release', False, True),
            ('fast-debug', 'Debug', False, True)]
rows = []
environment = os.environ.copy()
environment['ASAN_OPTIONS'] = 'detect_leaks=0:halt_on_error=1'
environment['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
for name, configuration, sanitizers, fast_fp in profiles:
    output = art / name
    output.mkdir()
    build = args.build_base / name
    assert not build.exists(), build
    commands = [('configure', ['cmake', '-S', str(source / 'tests/native/vgui_font_services/component'),
        '-B', str(build), '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=' + configuration,
        '-DCMAKE_OSX_ARCHITECTURES=arm64', '-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0',
        '-DCSE_FONT_SANITIZERS=' + ('ON' if sanitizers else 'OFF'),
        '-DCSE_FONT_FAST_FP=' + ('ON' if fast_fp else 'OFF')]),
        ('build', ['cmake', '--build', str(build), '--parallel', '2']),
        ('tests', ['ctest', '--test-dir', str(build), '--output-on-failure']
            + (['--exclude-regex', '^font-module-repeated$'] if sanitizers else []))]
    row = {'profile': name, 'phases': []}
    for phase, command in commands:
        result = run(command, output / (phase + '.log'), environment)
        row['phases'].append(result)
        if result['exit']:
            break
    if len(row['phases']) == 3 and row['phases'][-1]['exit'] == 0:
        shutil.copy2(build / 'compile_commands.json', output / 'compile_commands.json')
        images = ['libfont_component.dylib', 'libfont_consumer_a.dylib', 'libfont_consumer_b.dylib',
                  'font_component_tests', 'font_prepare_tests', 'font_misuse_tests', 'font_module_lifecycle',
                  'libfont_reload_leaf.dylib', 'libfont_reload_virtual.dylib',
                  'libfont_reload_client_a.dylib', 'libfont_reload_client_b.dylib', 'font_reload_control']
        row['binaries'] = {}
        for image in images:
            shutil.copy2(build / image, output / image)
            row['binaries'][image] = sha(output / image)
        row['identity'] = run(['file'] + [str(output / image) for image in images], output / 'machine.log')
        row['load_commands'] = run(['otool', '-l', str(output / 'libfont_component.dylib')],
                                   output / 'load-commands.log')
        row['imports'] = run(['otool', '-L', str(output / 'libfont_component.dylib')], output / 'imports.log')
        for receipt in ('module-teardown.txt', 'repeat-teardown.txt', 'exit-teardown.txt'):
            if (build / receipt).exists():
                shutil.copy2(build / receipt, output / receipt)
        plain_control = run([str(build / 'font_reload_control'), str(build / 'libfont_reload_leaf.dylib')],
                            output / 'plain-reload-control.log', environment)
        virtual_control = run([str(build / 'font_reload_control'), str(build / 'libfont_reload_virtual.dylib'),
            str(build / 'libfont_reload_client_a.dylib'), str(build / 'libfont_reload_client_b.dylib')],
            output / 'virtual-reload-control.log', environment)
        row['independent_controls'] = [plain_control, virtual_control]
        assert plain_control['exit'] == 0
        if sanitizers:
            assert virtual_control['exit'] != 0
            assert '__asan_register_globals' in (output / 'virtual-reload-control.log').read_text()
            listing = json.loads(subprocess.check_output(
                ['ctest', '--test-dir', str(build), '--show-only=json-v1'], text=True))
            repeated = next(x['command'] for x in listing['tests'] if x['name'] == 'font-module-repeated')
            row['instrumented_repeated_red'] = run(repeated, output / 'instrumented-repeated-red.log', environment)
            assert row['instrumented_repeated_red']['exit'] != 0
            assert '__asan_register_globals' in (output / 'instrumented-repeated-red.log').read_text()
        else:
            assert virtual_control['exit'] == 0
    rows.append(row)
    (art / 'cases.json').write_text(json.dumps(rows, indent=2) + '\n')
    print(name, [(x['exit']) for x in row['phases']], flush=True)

# Keep a missing-binding link failure. It proves that fixture provider symbols
# cannot become an accidental production default provider or Surface stub.
missing = art / 'missing-bindings'
missing.mkdir()
missing_build = args.build_base / 'missing-bindings'
missing_results = [run(['cmake', '-S', str(source / 'tests/native/vgui_font_services/component'),
    '-B', str(missing_build), '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release',
    '-DCMAKE_OSX_ARCHITECTURES=arm64', '-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0',
    '-DCSE_FONT_FIXTURE_BINDINGS=OFF'], missing / 'configure.log')]
missing_results.append(run(['cmake', '--build', str(missing_build), '--target', 'font_component'],
                           missing / 'build.log'))
missing_text = (missing / 'build.log').read_text()
assert missing_results[0]['exit'] == 0 and missing_results[1]['exit'] != 0, missing_results
assert 'CseVgui1QualifiedFontProviderV1' in missing_text and 'CseVgui1SelectFontForSurfaceV1' in missing_text
(art / 'missing-bindings.json').write_text(json.dumps(missing_results, indent=2) + '\n')

selected = set()
for name, _, _, _ in profiles:
    build = args.build_base / name
    if not (build / 'build.ninja').exists():
        continue
    for line in subprocess.check_output(['ninja', '-C', str(build), '-t', 'deps'], text=True).splitlines():
        path = Path(line.strip())
        if path.is_file() and source not in path.parents:
            selected.add(path)
(art / 'compiler-input-hashes.json').write_text(json.dumps(
    {str(path): sha(path) for path in sorted(selected)}, indent=2) + '\n')
assert all(sha(source / name) == digest for name, digest in hashes.items())
raise SystemExit(int(any(len(x['phases']) != 3 or x['phases'][-1]['exit'] for x in rows)))
