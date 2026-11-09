"""Extract the exact current platform allocation expressions; never rewrite production."""

import argparse
import hashlib
import json
from pathlib import Path
import re


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    raw = args.source.read_bytes()
    text = raw.decode("utf-8")
    limit = re.search(r"constexpr unsigned int kPcmCountLimit = (0x[0-9A-Fa-f]+u);", text)
    block = re.search(r"#if defined\(_WIN32\)\s*\n(\s*pcm = new short\[sampleCount\];)\s*"
                      r"#else\s*\n(.*?)\s*#endif", text, re.S)
    if not limit or not block:
        raise SystemExit("actual PCM allocation block or count limit changed; review extraction")
    args.output.mkdir(parents=True, exist_ok=False)
    result = '#include "allocation_investigation.h"\n#include <new>\n'
    result += "constexpr unsigned int kPcmCountLimit = " + limit.group(1) + ";\n"
    for name, body in (("ReferenceWindowsAllocation", block.group(1)), ("ReferenceNativeAllocation", block.group(2))):
        result += "short* " + name + "(unsigned int sampleCount) {\n    short* pcm = nullptr;\n"
        result += body + "\n    return pcm;\n}\n"
    (args.output / "allocation_reference.cpp").write_text(result)
    (args.output / "source.json").write_text(json.dumps({
        "source": str(args.source.resolve()), "sha256": hashlib.sha256(raw).hexdigest(),
        "allocation_block": block.group(0), "count_limit": limit.group(0),
        "scope": "Exact current Windows/native allocation expressions, controlled throwing allocator; no media/SDL"
    }, indent=2) + "\n")


if __name__ == "__main__":
    main()
