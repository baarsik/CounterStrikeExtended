"""Recompile one owned TU and full-link SDL with pinned qualified cache inputs.

Does not invoke a game/probe, configure a dependency, or modify the input cache.
Run each configuration sequentially (one compiler process per invocation).
"""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import tarfile

parser = argparse.ArgumentParser()
parser.add_argument('--repo', type=Path, required=True)
parser.add_argument('--cache', type=Path, required=True)
parser.add_argument('--packet', type=Path, required=True)
parser.add_argument('--config', choices=['Release', 'Debug'], required=True)
args = parser.parse_args()
args.repo = args.repo.resolve()
args.cache = args.cache.resolve()
args.packet = args.packet.resolve()
args.packet.mkdir(parents=True, exist_ok=False)
commands = []


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(name, argv, cwd):
    result = subprocess.run([str(value) for value in argv], cwd=cwd, capture_output=True, text=True, timeout=120)
    (args.packet / (name + '.log')).write_text(result.stdout + result.stderr)
    commands.append(dict(name=name, argv=[str(value) for value in argv], cwd=str(cwd), exit=result.returncode))
    (args.packet / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
    if result.returncode:
        raise RuntimeError(f'{name}: exit {result.returncode}; see packet log')
    return result.stdout


source = args.repo / 'GoldSrc-SDK/common/sdl3_native_cocoa_mouse.mm'
entries = json.loads((args.cache / 'compile_commands.json').read_text())
owner = next(entry for entry in entries if entry['file'].endswith('/sdl3_native_cocoa_mouse.mm'))
compile_argv = shlex.split(owner['command'])
old_object = compile_argv[compile_argv.index('-o') + 1]
new_object = args.packet / 'sdl3_native_cocoa_mouse.mm.o'
compile_argv[compile_argv.index('-o') + 1] = str(new_object)
compile_argv[compile_argv.index('-c') + 1] = str(source)
compile_argv += ['-MMD', '-MF', str(args.packet / 'provider.d')]

target = f'dependencies/SDL3/lib/{args.config}/libSDL3.0.dylib'
recipe = run('retained-link-recipe', ['ninja', '-t', 'commands', target], args.cache).splitlines()[-1]
link_argv = shlex.split(recipe)
assert link_argv[:2] == [':', '&&'] and link_argv[-2:] == ['&&', ':']
link_argv = link_argv[2:-2]
assert old_object in link_argv
unchanged = [args.cache / value for value in link_argv if value.endswith(('.o', '.a')) and value != old_object]
before = {str(path): digest(path) for path in unchanged}
assert unchanged
dependency_text = run('retained-object-dependencies', ['ninja', '-t', 'deps'] +
    [str(path.relative_to(args.cache)) for path in unchanged if path.suffix == '.o'], args.cache)
dependencies = set()
for line in dependency_text.splitlines():
    if line.startswith('    '):
        path = Path(line.strip())
        path = path if path.is_absolute() else args.cache / path
        if path.is_file():
            dependencies.add(path.resolve())
assert dependencies
dependency_pins = {str(path): digest(path) for path in sorted(dependencies)}

archive = args.repo / 'GoldSrc-SDK/external/SDL3-source/SDL3-3.4.20.tar.gz'
assert digest(archive) == 'f3bb06f55bed9557227a700af2ea62e49497ebdf58037732a731a5fb3ef8581f'
upstream = args.cache / 'dependencies/SDL3/source'
upstream_pins = {}
with tarfile.open(archive) as bundle:
    for member in bundle.getmembers():
        if member.isfile():
            expected = hashlib.sha256(bundle.extractfile(member).read()).hexdigest()
            path = upstream / member.name
            assert digest(path) == expected, path
            upstream_pins[member.name] = expected

image = args.packet / 'libSDL3.0.dylib'
link_argv[link_argv.index('-o') + 1] = str(image)
link_argv[link_argv.index(old_object)] = str(new_object)
run('compile-owned-provider', compile_argv, args.cache)
run('full-link-provider', link_argv, args.cache)
assert before == {str(path): digest(path) for path in unchanged}
assert dependency_pins == {str(path): digest(Path(path)) for path in dependency_pins}
symbols = run('exports', ['nm', '-gU', image], args.packet)
for export in ['CheckCocoaMouseOwned', 'CheckCocoaRelativeFocus', 'ReadCocoaMouseAbsolute',
               'SetCocoaGameplayCursor', 'CheckCocoaInteractionOwner', 'SetCocoaGameplayShortcuts']:
    assert '_CseSdl_' + export + 'V1' in symbols
run('sign-provider', ['codesign', '--force', '--sign', '-', image], args.packet)
run('provider-signature', ['codesign', '--display', '--verbose=4', image], args.packet)
run('image-policy', ['otool', '-L', '-l', image], args.packet)

probe = args.packet / 'provider_probe'
headers = args.cache / 'dependencies/SDL3/source/SDL3-3.4.20/include'
probe_argv = ['/usr/bin/clang++', '-std=c++14', '-arch', 'arm64', '-mmacosx-version-min=14.0',
    '-Wall', '-Wextra', '-Werror', '-fno-exceptions', '-funwind-tables', '-ffp-model=precise',
    '-ffp-contract=off', '-Wno-overriding-option', '-O2' if args.config == 'Release' else '-O0']
if args.config == 'Debug':
    probe_argv += ['-g']
probe_argv += ['-I' + str(headers), '-I' + str(source.parent),
    str(args.repo / 'tests/native/gameplay_shortcuts/provider_probe.cpp'), str(image),
    '-Wl,-rpath,@executable_path', '-o', str(probe)]
run('build-video-only-probe', probe_argv, args.packet)
run('sign-probe', ['codesign', '--force', '--sign', '-', probe], args.packet)
run('probe-signature', ['codesign', '--display', '--verbose=4', probe], args.packet)
run('probe-dependencies', ['otool', '-L', probe], args.packet)
source_pins = {str(path.relative_to(args.repo)): digest(path) for path in [source,
    source.with_suffix('.h'), source.parent / 'sdl3_native_cocoa_keyboard.h',
    source.parent / 'sdl3_native_cocoa_keyboard_private.h', source.parent / 'sdl3_native_cocoa_mouse_private.h',
    source.parent / 'sdl3_native_mouse_bridge.h', source.parent / 'sdl3_native_mouse_bridge.c',
    args.repo / 'tests/native/gameplay_shortcuts/provider_probe.cpp']}
receipt = dict(config=args.config, source=source_pins, archive_sha256=digest(archive),
    upstream_files=upstream_pins, unchanged_objects=before, unchanged_dependencies=dependency_pins,
    images={path.name: digest(path) for path in [image, new_object, probe]},
    commands=commands, probe_launched=False, app_modified=False)
(args.packet / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
print(f'{args.config}: one owned TU compiled, full SDL linked, {len(before)} input objects/archives unchanged, '
      f'{len(dependency_pins)} dependency pins, {len(upstream_pins)} upstream files unchanged; probe built only')
