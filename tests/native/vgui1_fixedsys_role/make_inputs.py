"""Authored geometry only, no existing font inputs."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys
sys.dont_write_bytecode = True


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def glyph(contours):
    if not contours:
        return b''
    points = [point for contour in contours for point in contour]
    xs, ys = zip(*points)
    output = struct.pack('>5h', len(contours), min(xs), min(ys), max(xs), max(ys))
    count = 0
    for contour in contours:
        count += len(contour)
        output += struct.pack('>H', count - 1)
    output += struct.pack('>H', 0) + bytes([1] * len(points))
    for values in (xs, ys):
        prior = 0
        for value in values:
            output += struct.pack('>h', value - prior)
            prior = value
    return output + bytes(len(output) % 2)


def cmap(mapping):
    pairs = sorted(mapping.items()) + [(0xffff, 0)]
    count = len(pairs)
    power = count.bit_length() - 1
    search = 2 << power
    data = struct.pack('>7H', 4, 16 + count * 8, 0, count * 2, search, power, count * 2 - search)
    data += struct.pack(f'>{count}H', *(unit for unit, _ in pairs)) + bytes(2)
    data += struct.pack(f'>{count}H', *(unit for unit, _ in pairs))
    data += struct.pack(f'>{count}H', *((index - unit) & 0xffff for unit, index in pairs)) + bytes(count * 2)
    return struct.pack('>2H2HI', 0, 1, 0, 3, 12) + data


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    parent = Path(__file__).resolve().parents[1]
    base = load('owned_geometry', parent / 'vgui1_legacy_raster/make_owned_font.py')
    util = load('owned_native_packer', parent / 'vgui1_native_provider/make_inputs.py')
    original, metadata = base.make()
    tables = util.tables_of(original)
    tables.pop(b'OS/2')
    tables[b'name'] = util.names('Courier', 'Regular', 'Courier')
    shapes = [[[ (0, 0), (0, 700), (512, 700), (512, 0)],
               [(100, 100), (400, 100), (400, 600), (100, 600)]], []]
    shapes += [[shape] if shape else [] for shape in metadata['glyphs'][2:]]
    shapes += [[[(40, 100), (40, 500), (320, 500), (320, 100)]] for _ in range(4)]
    advances = [640] * len(shapes)
    advances[-4:] = [400, 560, 590, 560]
    def rebuilt(changed_shapes, changed_tables):
        result = dict(changed_tables)
        data = b''
        offsets = []
        for contours in changed_shapes:
            offsets.append(len(data)); data += glyph(contours)
        offsets.append(len(data))
        result[b'glyf'] = data
        result[b'loca'] = struct.pack(f'>{len(offsets)}I', *offsets)
        result[b'hmtx'] = b''.join(struct.pack('>Hh', advance, 0) for advance in advances)
        hhea = bytearray(result[b'hhea']); struct.pack_into('>H', hhea, 34, len(changed_shapes)); result[b'hhea'] = bytes(hhea)
        maxp = bytearray(result[b'maxp']); struct.pack_into('>H', maxp, 4, len(changed_shapes)); struct.pack_into('>H', maxp, 6, 8); struct.pack_into('>H', maxp, 8, 2); result[b'maxp'] = bytes(maxp)
        return result
    tables = rebuilt(shapes, tables)
    mapping = {}
    for byte, unit in enumerate(base.CP1252):
        if 0x20 <= byte < 0x7f or byte >= 0xa0 or (0x80 <= byte < 0xa0 and unit >= 0xa0):
            mapping[unit] = 2 + byte % 7
    mapping[32] = 1; mapping[160] = 1; mapping[8] = 1
    for index, byte in enumerate([0xb0, 0xb1, 0xb5, 0xf7]): mapping[base.CP1252[byte]] = 9 + index
    tables[b'cmap'] = cmap(mapping)
    outputs = {'role.ttf': tables}
    missing = dict(tables); removed = dict(mapping); removed.pop(ord('A')); missing[b'cmap'] = cmap(removed); outputs['missing-graphic.ttf'] = missing
    empty = dict(tables); changed = dict(mapping); changed[ord('A')] = 1; empty[b'cmap'] = cmap(changed); outputs['empty-graphic.ttf'] = empty
    blank = list(shapes); blank[0] = []; outputs['empty-notdef.ttf'] = rebuilt(blank, tables)
    italic = dict(tables); head = bytearray(tables[b'head']); struct.pack_into('>H', head, 44, 2); italic[b'head'] = bytes(head); post = bytearray(tables[b'post']); struct.pack_into('>i', post, 4, 65536); italic[b'post'] = bytes(post); outputs['italic.ttf'] = italic
    bold = dict(tables); head = bytearray(tables[b'head']); struct.pack_into('>H', head, 44, 1); bold[b'head'] = bytes(head); outputs['bold.ttf'] = bold
    gap = dict(tables); hhea = bytearray(tables[b'hhea']); struct.pack_into('>h', hhea, 8, 1); gap[b'hhea'] = bytes(hhea); outputs['linegap.ttf'] = gap
    short = dict(tables); short[b'hmtx'] = bytes(4); outputs['short-hmtx.ttf'] = short
    present = dict(tables); present[b'OS/2'] = util.tables_of(original)[b'OS/2']; outputs['with-os2.ttf'] = present
    unfit_shapes = list(shapes)
    unfit_shapes[2] = [[(0, 0), (0, 700), (30000, 700), (30000, 0)]]
    unfit = rebuilt(unfit_shapes, tables)
    head = bytearray(unfit[b'head'])
    struct.pack_into('>h', head, 40, 30000)
    unfit[b'head'] = bytes(head)
    outputs['unfit.ttf'] = unfit
    unequal = dict(tables)
    hmtx = bytearray(tables[b'hmtx'])
    struct.pack_into('>H', hmtx, 2 * 4, 641)
    unequal[b'hmtx'] = bytes(hmtx)
    outputs['nonuniform-ascii.ttf'] = unequal
    post_only = dict(tables)
    post = bytearray(tables[b'post'])
    struct.pack_into('>i', post, 4, 65536)
    post_only[b'post'] = bytes(post)
    outputs['post-italic.ttf'] = post_only
    args.directory.mkdir(parents=True, exist_ok=True)
    manifest = {}
    for name, value in outputs.items():
        data = util.pack(value, base.checksum)
        (args.directory / name).write_bytes(data)
        manifest[name] = {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
    (args.directory / 'inputs.json').write_text(json.dumps(manifest, indent=2) + '\n')


if __name__ == '__main__':
    main()
