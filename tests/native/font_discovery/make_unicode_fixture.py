"""Rename a local system TrueType font for a process-only discovery fixture.

No system file is changed. The derived font stays in the test build directory
and must not be distributed. Raster data and metrics are preserved verbatim.
"""

from pathlib import Path
import struct
import sys


def checksum(data):
    padded = data + bytes((-len(data)) % 4)
    return sum(struct.unpack(f'>{len(padded) // 4}I', padded)) & 0xffffffff


def rename_font(source, destination):
    data = source.read_bytes()
    version, count, search_range, selector, range_shift = struct.unpack_from('>I4H', data)
    if version != 0x00010000:
        raise ValueError('fixture source must be a TrueType sfnt, not a collection')
    tables = {}
    for index in range(count):
        tag, _, offset, length = struct.unpack_from('>4s3I', data, 12 + index * 16)
        tables[tag] = data[offset:offset + length]
    # Only names used for family/style matching change; retain copyright,
    # licensing and other metadata from the local system source.
    replacement = {
        1: 'CSE 字体 Fixture with a long Unicode family name', 2: 'Regular',
        4: 'CSE 字体 Fixture with a long Unicode family name Regular',
        6: 'CSEUnicodeFixture-Regular', 16: 'CSE 字体 Fixture with a long Unicode family name', 17: 'Regular',
    }
    old_name = tables[b'name']
    _, name_count, string_offset = struct.unpack_from('>3H', old_name)
    records = []
    strings = bytearray()
    for index in range(name_count):
        platform, encoding, language, name_id, length, offset = struct.unpack_from('>6H', old_name, 6 + index * 12)
        if name_id not in replacement:
            value = old_name[string_offset + offset:string_offset + offset + length]
            records.append((platform, encoding, language, name_id, len(value), len(strings)))
            strings.extend(value)
    for name_id, value in replacement.items():
        encoded = value.encode('utf-16-be')
        records.append((3, 1, 0x0409, name_id, len(encoded), len(strings)))
        strings.extend(encoded)
    records.sort(key=lambda record: record[:4])
    tables[b'name'] = (struct.pack('>3H', 0, len(records), 6 + len(records) * 12)
                       + b''.join(struct.pack('>6H', *record) for record in records) + strings)
    head = bytearray(tables[b'head'])
    head[8:12] = bytes(4)
    tables[b'head'] = bytes(head)
    output = bytearray(struct.pack('>I4H', version, count, search_range, selector, range_shift))
    output.extend(bytes(count * 16))
    head_offset = None
    for index, (tag, value) in enumerate(sorted(tables.items())):
        output.extend(bytes((-len(output)) % 4))
        offset = len(output)
        struct.pack_into('>4s3I', output, 12 + index * 16, tag, checksum(value), offset, len(value))
        output.extend(value)
        if tag == b'head':
            head_offset = offset
    struct.pack_into('>I', output, head_offset + 8, (0xb1b0afba - checksum(output)) & 0xffffffff)
    destination.write_bytes(output)


if __name__ == '__main__':
    rename_font(Path(sys.argv[1]), Path(sys.argv[2]))
