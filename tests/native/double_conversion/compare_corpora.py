"""Compare every encoded result or captured binary64 value without normalization."""
from pathlib import Path
import argparse
import hashlib
import json
import struct


def compare(reference, candidate, include_scaled):
    report = {}
    for suffix, width in (("direct", 4), ("method", 4), ("scaled", 8)):
        if suffix == "scaled" and not include_scaled:
            continue
        expected = Path(str(reference) + "-" + suffix + ".bin").read_bytes()
        actual = Path(str(candidate) + "-" + suffix + ".bin").read_bytes()
        if len(expected) != len(actual) or len(expected) % width:
            raise ValueError("Incomplete or differently sized " + suffix + " corpus")
        encoding = "<I" if width == 4 else "<Q"
        differences = sum(a != b for a, b in zip(struct.iter_unpack(encoding, expected),
                                               struct.iter_unpack(encoding, actual)))
        report[suffix] = dict(records=len(expected) // width, differences=differences,
                              reference_sha256=hashlib.sha256(expected).hexdigest(),
                              candidate_sha256=hashlib.sha256(actual).hexdigest())
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("--scaled", action="store_true")
    arguments = parser.parse_args()
    result = compare(arguments.reference, arguments.candidate, arguments.scaled)
    print(json.dumps(result, indent=2))
    raise SystemExit(any(row["differences"] for row in result.values()))
