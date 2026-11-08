"""Build a local mixed-name TTC to test registration failure ownership."""

from pathlib import Path
import struct
import sys


def collection(paths):
    fonts = [bytearray(Path(path).read_bytes()) for path in paths]
    header_size = 12 + len(fonts) * 4
    offsets = []
    output = bytearray(header_size)
    for font in fonts:
        output.extend(bytes((-len(output)) % 4))
        offset = len(output)
        offsets.append(offset)
        count = struct.unpack_from('>H', font, 4)[0]
        for index in range(count):
            record = 12 + index * 16
            table_offset = struct.unpack_from('>I', font, record + 8)[0]
            if font[record:record + 4] == b'head':
                struct.pack_into('>I', font, table_offset + 8, 0)
            struct.pack_into('>I', font, record + 8, table_offset + offset)
        output.extend(font)
    struct.pack_into('>4sII', output, 0, b'ttcf', 0x00010000, len(fonts))
    struct.pack_into(f'>{len(fonts)}I', output, 12, *offsets)
    return output


if __name__ == '__main__':
    Path(sys.argv[3]).write_bytes(collection(sys.argv[1:3]))
