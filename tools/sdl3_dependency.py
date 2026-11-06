#!/usr/bin/env python3
"""Validate the vendored SDL3 payload without loading or executing it."""
from pathlib import Path
import argparse
import hashlib
import struct

ROOT = Path(__file__).resolve().parents[1]
VERSION = '3.4.20'
X86_MACHINE = 0x014c
MANIFEST_SHA256 = '75af41d550d677be367a481d95f1adc5145a331cc2c08abcf05fdb83908ddc9a'


def payload_hashes(root):
    manifest = root / 'SHA256SUMS'
    if not manifest.is_file():
        raise ValueError('Missing SDL3 payload manifest: ' + str(manifest))
    if hashlib.sha256(manifest.read_bytes()).hexdigest() != MANIFEST_SHA256:
        raise ValueError('SDL3 payload manifest hash mismatch')
    entries = {}
    for line in manifest.read_text().splitlines():
        digest, relative = line.split('|')
        if (len(digest) != 64 or any(c not in '0123456789abcdef' for c in digest)
                or Path(relative).is_absolute() or '..' in Path(relative).parts or relative in entries):
            raise ValueError('Invalid SDL3 payload manifest entry: ' + line)
        entries[relative] = digest
    required = {'include/SDL3/SDL.h', 'include/SDL3/SDL_version.h', 'lib/x86/SDL3.lib',
                'lib/x86/SDL3.dll', 'cmake/SDL3Config.cmake', 'cmake/SDL3ConfigVersion.cmake',
                'cmake/sdlcpu.cmake', 'LICENSE.txt', 'README.md', 'INSTALL.md'}
    if not required.issubset(entries):
        raise ValueError('Incomplete SDL3 payload manifest')
    return entries


def validate_artifact_identity(root):
    dll = (root / 'lib/x86/SDL3.dll').read_bytes()
    pe_offset = struct.unpack_from('<I', dll, 0x3c)[0]
    if (dll[:2] != b'MZ' or dll[pe_offset:pe_offset + 4] != b'PE\0\0'
            or struct.unpack_from('<H', dll, pe_offset + 4)[0] != X86_MACHINE):
        raise ValueError('SDL3.dll is not a Windows x86 PE')
    archive = (root / 'lib/x86/SDL3.lib').read_bytes()
    if archive[:8] != b'!<arch>\n':
        raise ValueError('SDL3.lib is not a COFF archive')
    cursor = 8
    imports = 0
    while cursor < len(archive):
        header = archive[cursor:cursor + 60]
        size = int(header[48:58])
        member = archive[cursor + 60:cursor + 60 + size]
        if member[:4] == b'\0\0\xff\xff':
            if struct.unpack_from('<H', member, 6)[0] != X86_MACHINE:
                raise ValueError('SDL3.lib contains a non-x86 import')
            strings = member[20:].split(b'\0')
            if len(strings) < 2 or strings[1] != b'SDL3.dll':
                raise ValueError('SDL3.lib has an unexpected DLL import identity')
            imports += 1
        cursor += 60 + size + size % 2
    if not imports:
        raise ValueError('SDL3.lib contains no native SDL3 imports')


def validate_payload(root):
    entries = payload_hashes(root)
    for relative, expected in entries.items():
        path = root / relative
        if not path.is_file():
            raise ValueError('Missing SDL3 payload: ' + str(path))
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise ValueError('SDL3 hash mismatch: ' + str(path))
    validate_artifact_identity(root)
    return entries


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT / 'GoldSrc-SDK/external/SDL3')
    args = parser.parse_args()
    try:
        entries = validate_payload(args.root)
    except (ValueError, OSError, struct.error) as error:
        parser.exit(1, str(error) + '\n')
    print(f'Validated SDL3 {VERSION}: {len(entries)} files; DLL/import machine x86; import identity SDL3.dll.')


if __name__ == '__main__':
    main()
