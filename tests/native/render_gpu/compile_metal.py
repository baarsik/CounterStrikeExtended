"""Pinned Windows/macOS Metal offline producer; no GPU, login or runtime compilation."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def verify_toolchain(root, pins):
    for row in pins['files']:
        path = root / row['path']
        if path.stat().st_size != row['bytes'] or sha256(path) != row['sha256']:
            raise ValueError('Pinned Metal toolchain input changed: ' + row['path'])
    for row in pins.get('symlinks', []):
        path = root / row['path']
        expected = root.resolve() / row['resolved']
        if not path.is_symlink() or os.readlink(path) != row['target'] or path.resolve() != expected:
            raise ValueError('Pinned Metal toolchain alias changed: ' + row['path'])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--toolchain', type=Path, required=True)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if os.name == 'nt':
        pins_name = 'metal_6_2_windows.json'
        compiler_relative = 'bin/metal.exe'
        linker_relative = 'bin/metallib.exe'
    elif sys.platform == 'darwin':
        pins_name = 'metal_27b47_macos.json'
        compiler_relative = 'usr/bin/metal'
        linker_relative = 'usr/bin/metallib'
    else:
        parser.error('This producer pins Windows Metal6.2 and macOS Metal27B47')
    if not args.output.is_absolute() or (os.name == 'nt' and args.output.drive.startswith('\\\\')):
        parser.error('Use a new local absolute output directory; Windows compiler inputs cannot use UNC paths')
    pins_path = Path(__file__).with_name(pins_name)
    pins = json.loads(pins_path.read_text())
    verify_toolchain(args.toolchain, pins)
    source_bytes = args.source.read_bytes()
    source_hash = hashlib.sha256(source_bytes).hexdigest()
    args.output.mkdir(exist_ok=False)
    source = args.output / 'legacy_first.metal'
    source.write_bytes(source_bytes)
    compiler = args.toolchain / compiler_relative
    linker = args.toolchain / linker_relative
    receipt = {'schema': 1, 'toolchain_pins_sha256': sha256(pins_path),
               'toolchain_pins': pins_name, 'source_sha256': source_hash, 'source': str(args.source),
               'steps': [], 'outputs': []}

    def run(label, command):
        result = subprocess.run(command, capture_output=True, timeout=45)
        log = args.output / (label + '.log')
        log.write_bytes(result.stdout + result.stderr)
        receipt['steps'].append({'label': label, 'command': command, 'exit': result.returncode,
                                 'log_sha256': sha256(log)})
        (args.output / 'compiler-receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
        print(label, result.returncode, flush=True)
        if result.returncode:
            raise RuntimeError('Metal compilation failed; see ' + str(log))
        return result.stdout.decode(errors='replace')

    version = run('version', [str(compiler), '--version'])
    if version.splitlines()[0] != pins['compiler_version']:
        raise ValueError('Pinned compiler version changed')
    common = ['-std=metal3.1', '-mmacosx-version-min=14.0', '-fno-fast-math', '-ffp-contract=off',
              '-fmetal-math-mode=safe', '-fmetal-math-fp32-functions=precise', '-Wall', '-Wextra', '-Werror']
    for configuration in ['Release', 'Debug']:
        directory = args.output / configuration
        directory.mkdir()
        flags = ['-O2'] if configuration == 'Release' else ['-O0', '-gline-tables-only']
        air = directory / 'legacy.air'
        reflection = directory / 'legacy.ll'
        run(configuration + '-compile', [str(compiler)] + common + flags +
            ['-c', str(source), '-o', str(air)])
        run(configuration + '-reflection', [str(compiler)] + common + flags +
            ['-S', '-emit-llvm', str(source), '-o', str(reflection)])
        for entry in ['LegacyVertex', 'LegacyFragment']:
            binary = directory / (entry + '.metallib')
            run(configuration + '-' + entry, [str(linker), '--function-name=' + entry,
                str(air), '-o', str(binary)])
            if binary.read_bytes()[:4] != b'MTLB':
                raise ValueError('Linker did not produce a metallib')
        for path in sorted(directory.iterdir()):
            receipt['outputs'].append({'path': path.relative_to(args.output).as_posix(),
                                       'bytes': path.stat().st_size, 'sha256': sha256(path)})
    verify_toolchain(args.toolchain, pins)
    if args.source.read_bytes() != source_bytes or source.read_bytes() != source_bytes:
        raise ValueError('Shader source changed during compilation')
    receipt['stable_inputs_verified'] = True
    (args.output / 'compiler-receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')


if __name__ == '__main__':
    main()
