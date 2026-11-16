"""Compile production Metal offline and emit a C++ byte array plus reproducible receipt."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--linker', type=Path, required=True)
    parser.add_argument('--configuration', choices=['Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel'], required=True)
    parser.add_argument('--deployment-target', default='14.0')
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    air = args.output.with_suffix('.air')
    library = args.output.with_suffix('.metallib')
    version = subprocess.check_output([str(args.compiler), '--version'], text=True)
    flags = ['-std=metal3.1', '-mmacosx-version-min=' + args.deployment_target,
             '-fno-fast-math', '-ffp-contract=off', '-Wall', '-Wextra', '-Werror']
    flags += ['-O0', '-gline-tables-only'] if args.configuration == 'Debug' else ['-O2']
    commands = [[str(args.compiler), *flags, '-c', str(args.source), '-o', str(air)],
                [str(args.linker), str(air), '-o', str(library)]]
    for command in commands:
        subprocess.run(command, check=True)
    data = library.read_bytes()
    if data[:4] != b'MTLB':
        raise RuntimeError('Metal linker did not produce a metallib')
    lines = [', '.join('0x%02x' % byte for byte in data[index:index + 16])
             for index in range(0, len(data), 16)]
    args.output.write_text('#pragma once\n#include <cstddef>\nnamespace cse { namespace render {\n'
                           'static const unsigned char kLegacyRuntimeMetal[] = {\n' +
                           ',\n'.join(lines) + '\n};\n} }\n')
    receipt = {'compiler_version': version.strip(), 'commands': commands,
               'source_sha256': hashlib.sha256(args.source.read_bytes()).hexdigest(),
               'metallib_sha256': hashlib.sha256(data).hexdigest(), 'configuration': args.configuration,
               'deployment_target': args.deployment_target}
    args.output.with_suffix('.json').write_text(json.dumps(receipt, indent=2) + '\n')


if __name__ == '__main__':
    main()
