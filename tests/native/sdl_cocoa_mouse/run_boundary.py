"""Owned hidden-window lifetime and genuine provider guard gate; no cursor read/input capture."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shlex
import shutil
import subprocess
import time
from inspect_images import inspect_image, ARM64_CPU, DYLIB_FILE_TYPE


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


parser = argparse.ArgumentParser()
parser.add_argument('--repo', type=Path, required=True)
parser.add_argument('--sdl-include', type=Path, required=True)
parser.add_argument('--provider', type=Path, required=True)
parser.add_argument('--variant', type=Path, required=True)
parser.add_argument('--frozen-sdk', type=Path, required=True)
parser.add_argument('--config', choices=['Release', 'Debug'], required=True)
parser.add_argument('--build', type=Path, required=True)
parser.add_argument('--evidence', type=Path, required=True)
args = parser.parse_args()
args.evidence.mkdir(parents=True, exist_ok=False)
records = []
inputs = {str(path): sha256(path) for path in [args.provider, args.variant]}


def receipt(success=False):
    data = dict(config=args.config, commands=records, provider_inputs=inputs, success=success,
                actual_owned_hidden_windows=True, explicit_cursor_read=False, input_capture=False,
                normal_vendor_video_initialization=True, full_engine_or_game_unload_qualified=False)
    (args.evidence / 'receipt.json').write_text(json.dumps(data, indent=2) + '\n')


def run(name, argv, expected=0, environment=None):
    start = time.monotonic()
    env = os.environ.copy()
    env.pop('SDL3_DYNAMIC_API', None)
    env.pop('SDL_VIDEO_DRIVER', None)
    env.update(environment or {})
    result = subprocess.run(list(map(str, argv)), stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            timeout=45, env=env)
    (args.evidence / (name + '.log')).write_bytes(result.stdout)
    records.append(dict(name=name, argv=list(map(str, argv)), expected=expected, exit=result.returncode,
                        seconds=time.monotonic() - start, environment=environment or {}))
    receipt()
    assert result.returncode == expected, records[-1]
    return result.stdout


run('configure', ['cmake', '-S', args.repo / 'tests/native/sdl_cocoa_mouse', '-B', args.build, '-G', 'Ninja',
    '-DCMAKE_BUILD_TYPE=' + args.config, '-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0',
    '-DCSE_SDL_INCLUDE=' + str(args.sdl_include), '-DCSE_SDL_PROVIDER=' + str(args.provider),
    '-DCSE_FROZEN_SAMPLER=' + str(args.frozen_sdk)])
run('build', ['cmake', '--build', args.build, '--parallel', '2'])
run('exports', [args.build / 'cocoa_exports_probe', args.provider])
run('c-import', [args.build / 'cocoa_c_import'])
run('cpp-adapter', [args.build / 'cocoa_adapter_tests'])
adapter = args.repo / 'src/engine/native_mouse_cocoa.cpp'
mutant_text, replacements = re.subn(r'==\s*CseSdlCocoaMouseReady',
    '!= CseSdlCocoaMouseNotReady', adapter.read_text())
assert replacements == 3
mutant = args.evidence / 'truthy-adapter.cpp'
mutant.write_text(mutant_text)
commands = json.loads((args.build / 'compile_commands.json').read_text())
command = next(row for row in commands if row['file'] == str(adapter)
               and 'cocoa_adapter_tests.dir' in row['command'])
compile_argv = shlex.split(command['command'])
mutant_object = args.evidence / 'truthy-adapter.o'
compile_argv[compile_argv.index('-o') + 1] = str(mutant_object)
compile_argv[compile_argv.index('-c') + 1] = str(mutant)
run('truthy-adapter-compile', compile_argv)
mutant_image = args.evidence / 'truthy-adapter'
run('truthy-adapter-link', ['/usr/bin/c++', '-arch', 'arm64', '-mmacosx-version-min=14.0', mutant_object,
    args.build / 'CMakeFiles/cocoa_adapter_tests.dir/adapter_tests.cpp.o', '-o', mutant_image])
run('truthy-adapter-red', [mutant_image], 17)
run('cpp-unload', [args.build / 'cocoa_module_host', args.provider, args.build / 'libcpp_consumer.dylib'])
run('old-objc-unload-red', [args.build / 'cocoa_module_host', args.provider, args.build / 'libold_objc_consumer.dylib'], 17)
run('same-image', [args.build / 'cocoa_provider_guards', 'same', args.provider],
    environment={'SDL_VIDEO_DRIVER': 'dummy'})
for mode in ['preinit', 'foreign']:
    log = run(mode, [args.build / 'cocoa_provider_guards', mode, args.variant, args.provider],
              environment={'SDL_VIDEO_DRIVER': 'dummy', 'SDL3_DYNAMIC_API': str(args.provider)})
    for phase in [('COCOA_PREINIT_BEGIN', 'COCOA_PREINIT_END')] if mode == 'preinit' else [
            ('COCOA_FOREIGN_BEGIN', 'COCOA_FOREIGN_END')]:
        begin = ('CSE_BRIDGE_PHASE:' + phase[0] + '\n').encode()
        end = ('CSE_BRIDGE_PHASE:' + phase[1] + '\n').encode()
        assert log.count(begin) == 1 and log.count(end) == 1
        assert log.split(begin)[1].split(end)[0] == b'', 'query region emitted output'

image_dir = args.evidence / 'images'
image_dir.mkdir()
image_inputs = [args.provider, args.variant] + [path for path in args.build.iterdir()
    if path.is_file() and (path.suffix == '.dylib' or path.name.startswith('cocoa_')) and path.suffix != '.txt']
images = []
for image in sorted(image_inputs):
    target = image_dir / image.name
    assert not target.exists()
    shutil.copy2(image, target)
    metadata = inspect_image(image)
    assert metadata['cpu'] == ARM64_CPU
    if image.suffix == '.dylib':
        assert metadata['filetype'] == DYLIB_FILE_TYPE
    if image.name == 'libcpp_consumer.dylib':
        assert not metadata['objc_metadata'] and not metadata['tlv_metadata']
    if image.name == 'libold_objc_consumer.dylib' or image == args.provider:
        assert metadata['objc_metadata']
    images.append(dict(path=str(image), sha256=sha256(image), bytes=image.stat().st_size,
                       archive=target.relative_to(args.evidence).as_posix(), macho=metadata))
    for suffix, options in [('headers', ['-hv']), ('loadcommands', ['-l']), ('dependencies', ['-L'])]:
        run(image.name + '-' + suffix, ['/usr/bin/otool', *options, image])
    run(image.name + '-exports', ['/usr/bin/nm', '-gUj', image])
for path, digest in inputs.items():
    assert sha256(Path(path)) == digest, 'provider input changed during gate'
(args.evidence / 'images.json').write_text(json.dumps(images, indent=2) + '\n')
shutil.copy2(args.build / 'compile_commands.json', args.evidence / 'compile_commands.json')
receipt(True)
print(args.config + ': C ABI, guards, foreign dispatch and three actual C++ unload epochs pass', flush=True)
