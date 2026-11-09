"""Author an HPAK v1 Win32 record independently of C++ native layouts."""
import hashlib
import struct
import sys
from pathlib import Path


def make_fixture():
    payload = bytes((index * 37 + 11) & 255 for index in range(1024))
    resource = b"{authored}\0".ljust(64, b"\0")
    resource += struct.pack("<iiiB", 3, 2, len(payload), 4)
    resource += hashlib.md5(payload).digest()
    resource += bytes([5]) + bytes(range(32))
    resource += bytes(2) + struct.pack("<II", 0, 0)
    assert len(resource) == 136
    record = resource + struct.pack("<ii", 12, len(payload))
    return b"HPAK" + struct.pack("<ii", 1, 12 + len(payload)) + payload + struct.pack("<i", 1) + record


if __name__ == "__main__":
    Path(sys.argv[1]).write_bytes(make_fixture())
