"""Compare actual RNG values with the pinned Win32 Release/Debug corpus."""
import hashlib
import re
import struct
import subprocess
import sys

# 27 seed/clock sequences: 512 ran1 words, 512 binary32 results and
# nine 512-value RandomLong ranges. All six original MSVC CPU/config pairs agree.
EXPECTED_COUNT = 152064
EXPECTED_WORD_SHA256 = "9b360baed1961055bc52e3293e19f1e2cf6d20fff0544c3c5cf0a40b19092273"


def verify(data):
    lines = data.splitlines()
    if len(lines) != EXPECTED_COUNT:
        raise ValueError(f"Expected {EXPECTED_COUNT} RNG values, got {len(lines)}")
    words = bytearray()
    for position, line in enumerate(lines):
        match = re.fullmatch(rb"([IFR]) (\d+) (\d+) ([0-9a-f]{8})", line)
        if not match:
            raise ValueError(f"Malformed RNG value at {position}: {line!r}")
        sequence, offset = divmod(position, 5632)
        kind = b"I" if offset < 512 else b"F" if offset < 1024 else b"R"
        index = offset if kind == b"I" else offset - 512 if kind == b"F" else offset - 1024
        if (match[1], int(match[2]), int(match[3])) != (kind, sequence, index):
            raise ValueError(f"Wrong RNG value order at {position}: {line!r}")
        words.extend(struct.pack("<I", int(match[4], 16)))
    digest = hashlib.sha256(words).hexdigest()
    if digest != EXPECTED_WORD_SHA256:
        raise ValueError(f"RNG values differ from Win32 corpus: {digest}")
    return len(lines), digest


if __name__ == "__main__":
    result = subprocess.run([sys.argv[1]], stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=15)
    if result.returncode:
        sys.stderr.buffer.write(result.stderr)
        raise SystemExit(f"Actual RNG fixture exited {result.returncode}")
    count, digest = verify(result.stdout)
    print(f"All {count} RNG values match Win32: {digest}")
