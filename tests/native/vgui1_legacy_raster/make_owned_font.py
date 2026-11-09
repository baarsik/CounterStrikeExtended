"""Build a redistributable authored SFNT from plain geometry; no input font exists."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

FAMILY = 'CSE Owned Raster V1'
POSTSCRIPT = 'CSEOwnedRasterV1-Regular'
UNITS = 1024
CP1252 = list(range(256))
for byte, unit in {0x80: 0x20ac, 0x82: 0x201a, 0x83: 0x0192, 0x84: 0x201e, 0x85: 0x2026,
    0x86: 0x2020, 0x87: 0x2021, 0x88: 0x02c6, 0x89: 0x2030, 0x8a: 0x0160, 0x8b: 0x2039,
    0x8c: 0x0152, 0x8e: 0x017d, 0x91: 0x2018, 0x92: 0x2019, 0x93: 0x201c, 0x94: 0x201d,
    0x95: 0x2022, 0x96: 0x2013, 0x97: 0x2014, 0x98: 0x02dc, 0x99: 0x2122, 0x9a: 0x0161,
    0x9b: 0x203a, 0x9c: 0x0153, 0x9e: 0x017e, 0x9f: 0x0178}.items():
    CP1252[byte] = unit


def checksum(data):
    padded = data + bytes((-len(data)) % 4)
    return sum(struct.unpack(f'>{len(padded) // 4}I', padded)) & 0xffffffff


def glyph(points):
    if not points:
        return b''  # Equal loca offsets encode the empty glyph without a truncated record.
    xs, ys = zip(*points)
    data = struct.pack('>5h', 1, min(xs), min(ys), max(xs), max(ys))
    data += struct.pack('>2H', len(points) - 1, 0)  # One contour, no instructions.
    data += bytes([1] * len(points))
    for axis in (xs, ys):
        previous = 0
        for value in axis:
            data += struct.pack('>h', value - previous)
            previous = value
    return data + bytes(len(data) % 2)


def make():
    shapes = [[(0, 0), (0, 700), (512, 700), (512, 0)], [],
        [(0, 0), (0, 700), (512, 700), (512, 0)],
        [(-128, 0), (-128, 700), (400, 700), (400, 0)],
        [(64, 0), (64, 700), (800, 700), (800, 0)],
        [(0, 0), (256, 700), (512, 0)],
        [(64, 0), (64, 700), (128, 700), (128, 0)],
        [(64, -180), (64, 600), (512, 600), (512, -180)],
        [(0, 0), (0, 256), (512, 700), (512, 0)]]
    advances = [640, 512, 640, 640, 640, 640, 640, 640, 768]
    bearings = [0, 0, 0, -128, 64, 0, 64, 64, 0]
    glyf = b''
    locations = []
    for shape in shapes:
        locations.append(len(glyf))
        glyf += glyph(shape)
    locations.append(len(glyf))
    tables = {b'glyf': glyf, b'loca': struct.pack(f'>{len(locations)}I', *locations),
              b'hmtx': b''.join(struct.pack('>Hh', width, left) for width, left in zip(advances, bearings))}
    tables[b'head'] = struct.pack('>4I2H2Q4h3H2h', 0x00010000, 0x00010000, 0, 0x5f0f3cf5,
        3, UNITS, 0, 0, -128, -180, 800, 700, 0, 8, 2, 1, 0)
    tables[b'hhea'] = struct.pack('>I3hH11hH', 0x00010000, 800, -224, 0, max(advances),
        min(bearings), -160, 800, 1, 0, 0, 0, 0, 0, 0, 0, len(shapes))
    tables[b'maxp'] = struct.pack('>I14H', 0x00010000, len(shapes), 4, 1, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0)
    os2 = bytearray(78)
    for offset, value in {0: 0, 2: 640, 4: 400, 6: 5, 10: 650, 12: 600, 16: 75,
        18: 650, 20: 600, 24: 350, 26: 50, 28: 300, 62: 64, 64: 0, 66: max(CP1252),
        68: 800, 72: 0, 74: 800, 76: 224}.items():
        struct.pack_into('>H', os2, offset, value)
    struct.pack_into('>h', os2, 70, -224)
    os2[32:42] = bytes([2, 11, 6, 3, 5, 4, 2, 2, 2, 4])
    struct.pack_into('>I', os2, 42, 3)
    os2[58:62] = b'CSE '
    tables[b'OS/2'] = bytes(os2)
    mapping = {}
    byte_glyphs = []
    for byte, unit in enumerate(CP1252):
        index = 0 if byte == 0 else 1 if byte in (9, 32) else 2 + byte % 7
        mapping[unit] = index
        byte_glyphs.append(index)
    segments = [(unit, index) for unit, index in sorted(mapping.items())] + [(0xffff, 0)]
    count = len(segments)
    selector = count.bit_length() - 1
    search = 2 << selector
    subtable = struct.pack('>7H', 4, 16 + 8 * count, 0, count * 2, search, selector, count * 2 - search)
    subtable += struct.pack(f'>{count}H', *(unit for unit, _ in segments)) + bytes(2)
    subtable += struct.pack(f'>{count}H', *(unit for unit, _ in segments))
    subtable += struct.pack(f'>{count}H', *((index - unit) & 0xffff for unit, index in segments)) + bytes(2 * count)
    tables[b'cmap'] = struct.pack('>2H2H I', 0, 1, 3, 1, 12) + subtable
    names = {1: FAMILY, 2: 'Regular', 3: 'CSE-authored-geometry-v1', 4: FAMILY + ' Regular',
             5: 'Version 1.0', 6: POSTSCRIPT, 16: FAMILY, 17: 'Regular'}
    records = b''
    text = b''
    for identifier, value in sorted(names.items()):
        encoded = value.encode('utf-16-be')
        records += struct.pack('>6H', 3, 1, 0x0409, identifier, len(encoded), len(text))
        text += encoded
    tables[b'name'] = struct.pack('>3H', 0, len(names), 6 + len(records)) + records + text
    tables[b'post'] = struct.pack('>Iihh5I', 0x00030000, 0, -100, 50, 0, 0, 0, 0, 0)
    tables[b'gasp'] = struct.pack('>4H', 1, 1, 0xffff, 0x000f)
    count = len(tables)
    selector = count.bit_length() - 1
    search = 16 << selector
    output = bytearray(struct.pack('>I4H', 0x00010000, count, search, selector, count * 16 - search))
    output.extend(bytes(count * 16))
    head = 0
    for index, (tag, value) in enumerate(sorted(tables.items())):
        output.extend(bytes((-len(output)) % 4))
        offset = len(output)
        struct.pack_into('>4s3I', output, 12 + index * 16, tag, checksum(value), offset, len(value))
        output.extend(value)
        if tag == b'head':
            head = offset
    output.extend(bytes((-len(output)) % 4))
    struct.pack_into('>I', output, head + 8, (0xb1b0afba - checksum(output)) & 0xffffffff)
    assert checksum(output) == 0xb1b0afba
    return output, {'family': FAMILY, 'postscript': POSTSCRIPT, 'units': UNITS, 'byte_to_unicode': CP1252,
                    'byte_to_glyph': byte_glyphs, 'glyphs': shapes, 'advances': advances, 'bearings': bearings}


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    args.directory.mkdir(parents=True, exist_ok=True)
    font, manifest = make()
    manifest['sha256'] = hashlib.sha256(font).hexdigest()
    (args.directory / 'owned-raster.ttf').write_bytes(font)
    (args.directory / 'owned-raster.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(manifest['sha256'], len(font))
