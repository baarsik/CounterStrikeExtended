#!/usr/bin/env python3
"""Require socket-v1 and factory dynamic publications in a final Apple executable.

Reads the actual Mach-O export trie, not the static symbol table, and never loads
or executes the image. Format constants/encoding follow Apple SDK mach-o/loader.h.
The separate owning ABI gate verifies the table records and typed callback roles.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

from binary_identity import require_architecture

MH_EXECUTE = 2
LC_SEGMENT_64 = 0x19
LC_DYLD_INFO = 0x22
LC_DYLD_INFO_ONLY = 0x80000022
LC_DYLD_EXPORTS_TRIE = 0x80000033
EXPORT_REEXPORT = 0x08
EXPORT_RESOLVER = 0x10
OWNERS = {'launcher': 'hl', 'hlds': 'hlds'}
# Extents are publication bounds, not a replacement for owning ABI validation.
REQUIRED = {'_CseNativeSocketHostV1': (48, 1), '_CreateInterface': (1, 5)}


def uleb(data, offset, end):
    value = 0
    for shift in range(0, 70, 7):
        if offset >= end:
            raise ValueError('Truncated export trie ULEB128')
        byte = data[offset]
        offset += 1
        value |= (byte & 0x7f) << shift
        if value >= 1 << 64:
            raise ValueError('Overflowing export trie ULEB128')
        if not byte & 0x80:
            return value, offset
    raise ValueError('Overlong export trie ULEB128')


def string(data, offset, end):
    stop = data.find(b'\0', offset, end)
    if stop < 0:
        raise ValueError('Unterminated export trie string')
    try:
        return data[offset:stop].decode('utf-8'), stop + 1
    except UnicodeDecodeError as error:
        raise ValueError('Invalid export trie UTF-8') from error


def decode_exports(trie):
    if not trie:
        return {}
    exports = {}
    pending = [(0, '', frozenset())]
    visits = 0
    while pending:
        node, prefix, ancestors = pending.pop()
        visits += 1
        if node in ancestors or node >= len(trie) or visits > len(trie) or len(prefix) > len(trie):
            raise ValueError('Invalid cyclic/out-of-range export trie')
        size, cursor = uleb(trie, node, len(trie))
        terminal_end = cursor + size
        if terminal_end >= len(trie):
            raise ValueError('Truncated export trie terminal/children')
        if size:
            if not prefix or prefix in exports:
                raise ValueError('Empty/duplicate export trie name')
            flags, cursor = uleb(trie, cursor, terminal_end)
            entry = {'flags': flags}
            if flags & EXPORT_REEXPORT:
                entry['ordinal'], cursor = uleb(trie, cursor, terminal_end)
                entry['import'], cursor = string(trie, cursor, terminal_end)
            else:
                entry['address'], cursor = uleb(trie, cursor, terminal_end)
                if flags & EXPORT_RESOLVER:
                    entry['resolver'], cursor = uleb(trie, cursor, terminal_end)
            if cursor != terminal_end:
                raise ValueError('Invalid export trie terminal size')
            exports[prefix] = entry
        cursor = terminal_end
        children = trie[cursor]
        cursor += 1
        for _ in range(children):
            suffix, cursor = string(trie, cursor, len(trie))
            if not suffix:
                raise ValueError('Empty export trie edge')
            child, cursor = uleb(trie, cursor, len(trie))
            pending.append((child, prefix + suffix, ancestors | {node}))
    return exports


def inspect_publication(data, architecture):
    identity = require_architecture(data, architecture, maximum_macos=(14, 0, 0))
    if data[:4] != b'\xcf\xfa\xed\xfe' or identity['format'] != 'mach-o':
        raise ValueError('Expected one thin little-endian Mach-O64 executable')
    file_type, count = struct.unpack_from('<II', data, 12)
    if file_type != MH_EXECUTE:
        raise ValueError('Main-image publication requires MH_EXECUTE file owner')
    cursor = 32
    segments = []
    export_ranges = []
    for _ in range(count):
        command, size = struct.unpack_from('<II', data, cursor)
        if command == LC_SEGMENT_64:
            address, memory_size, offset, file_size, _, protection = struct.unpack_from('<QQQQII', data, cursor + 24)
            segments.append((address, memory_size, offset, file_size, protection))
        elif command == LC_DYLD_EXPORTS_TRIE:
            if size != 16:
                raise ValueError('Invalid export-trie load command')
            export_ranges.append(struct.unpack_from('<II', data, cursor + 8))
        elif command in (LC_DYLD_INFO, LC_DYLD_INFO_ONLY):
            if size != 48:
                raise ValueError('Invalid dyld-info load command')
            offset, length = struct.unpack_from('<II', data, cursor + 40)
            if length:
                export_ranges.append((offset, length))
        cursor += size
    bases = [address for address, _, offset, size, protection in segments
             if offset == 0 and size >= 32 and protection & 1]
    if len(bases) != 1 or len(export_ranges) != 1:
        raise ValueError('Missing/ambiguous main-image header or dynamic export trie')
    offset, length = export_ranges[0]
    if not length or offset < cursor or offset + length > len(data):
        raise ValueError('Invalid export trie file extent')
    exports = decode_exports(data[offset:offset + length])
    for name, (extent, permissions) in REQUIRED.items():
        entry = exports.get(name)
        if entry is None:
            raise ValueError('Missing dynamic publication: ' + name)
        # Require a strong regular definition in this physical main image.
        # Re-exports, absolute/TLV symbols and resolver stubs cannot own the table.
        if entry['flags'] != 0:
            raise ValueError('Wrong-owner/non-regular dynamic publication: ' + name)
        address = bases[0] + entry['address']
        if not any(start <= address and address + extent <= start + min(memory_size, file_size)
                   and protection & permissions == permissions
                   for start, memory_size, _, file_size, protection in segments):
            raise ValueError('Dynamic publication is outside its file-backed main-image segment: ' + name)
    return {**identity, 'file_type': 'MH_EXECUTE', 'export_trie_offset': offset,
            'export_trie_size': length, 'exports': dict(sorted(exports.items()))}


def check_image(image, owner, architecture):
    if owner not in OWNERS or image.name != OWNERS[owner] or image.is_symlink():
        raise ValueError('Unexpected main-image owner output path: ' + str(image))
    data = image.read_bytes()
    return {'owner': owner, 'image': str(image.resolve()), 'image_sha256': hashlib.sha256(data).hexdigest(),
            **inspect_publication(data, architecture)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image', type=Path, required=True)
    parser.add_argument('--owner', choices=OWNERS, required=True)
    parser.add_argument('--architecture', choices=('arm64', 'x64'), required=True)
    parser.add_argument('--receipt', type=Path, required=True)
    args = parser.parse_args()
    # A failed recheck must not leave an earlier successful receipt authoritative.
    args.receipt.unlink(missing_ok=True)
    try:
        result = check_image(args.image, args.owner, args.architecture)
        args.receipt.parent.mkdir(parents=True, exist_ok=True)
        args.receipt.write_text(json.dumps(result, indent=2) + '\n')
        print(f"{args.owner}: final {args.architecture} MH_EXECUTE publishes socket-v1 and CreateInterface; "
              f"{len(result['exports'])} dynamic exports; SHA256 {result['image_sha256']}")
        return 0
    except (OSError, ValueError) as error:
        print(str(error), file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
