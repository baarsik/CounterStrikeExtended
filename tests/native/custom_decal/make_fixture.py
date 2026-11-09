"""Independent fixed12/32/40-byte WAD3 writer; never derives disk layout from C++ types."""
from pathlib import Path
import struct
import sys


def make_wad(width=16, height=16, first=40, colors=256, alpha=True, ordinary=False):
    pixels = width * height
    levels = [pixels >> (2 * index) for index in range(4)]
    offsets = [first]
    for level in levels[:-1]:
        offsets.append(offsets[-1] + level)
    total = sum(levels)
    lump_size = max(first + total + 2 + 768, 40 + total + 2)
    lump = bytearray((index * 13 + 17) & 255 for index in range(lump_size))
    name = b'ordinary\0'.ljust(16, b'\0') if ordinary else b'FIXTURE-MIP-NAME!'[:16]
    lump[:40] = name + struct.pack('<6I', width, height, *offsets)
    palette = first + total + 2
    lump[palette + 765:palette + 768] = bytes([0, 0, 255] if alpha else [17, 33, 65])
    # The retained validator/SW builder and HW bitmap use these two distinct addresses.
    struct.pack_into('<H', lump, first + total, colors)
    struct.pack_into('<H', lump, 40 + total, colors)
    directory = max(12 + len(lump), 1024 - 32)
    header = b'WAD3' + struct.pack('<ii', 1, directory)
    entry = struct.pack('<iii4B16s', 12, len(lump), len(lump), 67, 0, 0, 0, b'CUSTOM-LUMP-NAME!')
    return header + lump + bytes(directory - 12 - len(lump)) + entry


if __name__ == '__main__':
    output = Path(sys.argv[1])
    output.mkdir(parents=True, exist_ok=True)
    cases = {'canonical': {}, 'padded': {'first': 48}, 'odd': {'width': 3, 'height': 5},
             'count0': {'colors': 0}, 'count17-gradient': {'colors': 17, 'alpha': False},
             'overlap': {'first': 0}, 'unaligned': {'width': 1, 'height': 1},
             'ordinary': {'ordinary': True}, 'large': {'width': 256, 'height': 256}}
    for name, arguments in cases.items():
        (output / (name + '.wad')).write_bytes(make_wad(**arguments))
