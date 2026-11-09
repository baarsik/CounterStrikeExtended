"""Authored SFNT variants/collection; never reads a host font."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys

# Import the immutable geometry generator without writing beside its source.
sys.dont_write_bytecode = True


def tables_of(data):
    tables = {}
    for index in range(struct.unpack_from('>H', data, 4)[0]):
        tag, _, offset, length = struct.unpack_from('>4s3I', data, 12 + index * 16)
        tables[tag] = bytes(data[offset:offset + length])
    return tables


def pack(tables, checksum, absolute=0):
    count = len(tables)
    selector = count.bit_length() - 1
    search = 16 << selector
    out = bytearray(struct.pack('>I4H', 0x10000, count, search, selector, count * 16 - search))
    out += bytes(count * 16)
    head = 0
    for index, (tag, value) in enumerate(sorted(tables.items())):
        out += bytes((-len(out)) % 4)
        offset = len(out)
        if tag == b'head':
            value = value[:8] + bytes(4) + value[12:]
            head = offset
        struct.pack_into('>4s3I', out, 12 + index * 16, tag, checksum(value), absolute + offset, len(value))
        out += value
    out += bytes((-len(out)) % 4)
    if absolute == 0:
        struct.pack_into('>I', out, head + 8, (0xb1b0afba - checksum(out)) & 0xffffffff)
    return out


def names(family, style, ps):
    values = {1: family, 2: style, 3: 'CSE-authored-native-face-v1', 4: family + ' ' + style,
              5: 'Version 1.0', 6: ps, 16: family, 17: style}
    records = b''
    text = b''
    for identifier, value in sorted(values.items()):
        encoded = value.encode('utf-16-be')
        records += struct.pack('>6H', 3, 1, 0x409, identifier, len(encoded), len(text))
        text += encoded
    return struct.pack('>3H', 0, len(values), 6 + len(records)) + records + text


def symbol_cmap(mapping):
    segments = sorted(mapping.items()) + [(0xffff, 0)]
    count = len(segments)
    search = 2 << (count.bit_length() - 1)
    sub = struct.pack('>7H', 4, 16 + count * 8, 0, count * 2, search,
                      count.bit_length() - 1, count * 2 - search)
    sub += struct.pack(f'>{count}H', *(unit for unit, _ in segments)) + bytes(2)
    sub += struct.pack(f'>{count}H', *(unit for unit, _ in segments))
    sub += struct.pack(f'>{count}H', *((glyph - unit) & 0xffff for unit, glyph in segments)) + bytes(2 * count)
    return struct.pack('>2H2HI', 0, 1, 3, 0, 12) + sub


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1] / 'vgui1_legacy_raster/make_owned_font.py'
    spec = importlib.util.spec_from_file_location('owned_geometry', root)
    base = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(base)
    regular, metadata = base.make()
    tables = tables_of(regular)
    bold = dict(tables)
    bold[b'name'] = names(base.FAMILY, 'Bold', 'CSEOwnedRasterV1-Bold')
    os2 = bytearray(bold[b'OS/2'])
    struct.pack_into('>H', os2, 4, 700)
    struct.pack_into('>H', os2, 62, 32)
    bold[b'OS/2'] = bytes(os2)
    head = bytearray(bold[b'head'])
    struct.pack_into('>H', head, 44, 1)
    bold[b'head'] = bytes(head)
    outputs = {'regular.ttf': regular, 'bold.ttf': pack(bold, base.checksum)}
    for mode in ('direct', 'high', 'both', 'ambiguous', 'missing'):
        symbol = dict(tables)
        symbol[b'name'] = names('CSE Owned Symbol V1', 'Regular', 'CSEOwnedSymbolV1-Regular')
        mapping = {}
        for byte in range(1, 256):
            glyph = 2 + byte % 7
            if mode in ('direct', 'both', 'ambiguous'):
                mapping[byte] = glyph
            if mode in ('high', 'both', 'ambiguous', 'missing'):
                if mode == 'missing' and byte == ord('a'):
                    continue
                mapping[0xf000 + byte] = 2 + (byte + 1) % 7 if mode == 'ambiguous' else glyph
        symbol[b'cmap'] = symbol_cmap(mapping)
        outputs[f'symbol-{mode}.ttf'] = pack(symbol, base.checksum)
    # TTC directory offsets are absolute. No font dependency/tool rewrites input data.
    first = pack(tables, base.checksum, 20)
    second_offset = 20 + len(first)
    second = pack(bold, base.checksum, second_offset)
    outputs['indexed.ttc'] = struct.pack('>4s4I', b'ttcf', 0x10000, 2, 20, second_offset) + first + second
    args.directory.mkdir(parents=True, exist_ok=True)
    manifest = {}
    for name, data in outputs.items():
        (args.directory / name).write_bytes(data)
        manifest[name] = {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
    (args.directory / 'inputs.json').write_text(json.dumps(manifest, indent=2) + '\n')


if __name__ == '__main__':
    main()
