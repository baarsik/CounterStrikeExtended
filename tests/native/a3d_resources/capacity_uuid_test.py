"""Independently derive the private capacity UUID and check its C++ byte fields."""
from pathlib import Path
import re
import sys
import uuid

expected = uuid.uuid5(uuid.NAMESPACE_URL, "CounterStrikeExtended/IA3dNativeCapacityV1")
text = Path(sys.argv[1]).read_text().split("constexpr IID IID_IA3dNativeCapacityV1 =", 1)[1].split(";", 1)[0]
fields = [int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]+)", text)]
actual = uuid.UUID(fields=(fields[0], fields[1], fields[2], fields[3], fields[4],
                          int.from_bytes(bytes(fields[5:]), "big")))
assert actual == expected, (actual, expected)
print(f"PASS UUIDv5 URL capacity: {actual}")
