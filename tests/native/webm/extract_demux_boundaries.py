"""Extract actual SDK methods, substituting only the explicit realloc test binding."""

import argparse
import hashlib
import json
from pathlib import Path
import re


def function(text, name):
    start = text.index(name)
    # Include the declaration's return type when present.
    start = text.rfind("\n", 0, start) + 1
    opening = text.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    raw = args.source.read_bytes()
    text = raw.decode("utf-8")
    names = ["WebMFrame::WebMFrame()", "WebMFrame::~WebMFrame()",
             "WebMDemuxer::WebMDemuxer(", "WebMDemuxer::~WebMDemuxer()", "WebMDemuxer::ReadFrame("]
    bodies = [function(text, name) for name in names]
    if "StoreWebMFrameSize(" in text:
        bodies.insert(0, function(text, "StoreWebMFrameSize("))
    count = sum(body.count("std::realloc(") for body in bodies)
    if count != 1:
        raise SystemExit("realloc binding count changed; review exact extraction")
    constant = re.search(r"constexpr double kNanoseconds = [^;]+;", text).group(0)
    result = '#include "webm_video.h"\n#include <webm/mkvparser/mkvparser.h>\n'
    result += "#include <climits>\n#include <cstdlib>\n#include <cstring>\n#include <cstddef>\n"
    result += "void* FixtureReallocate(void*, std::size_t);\n"
    result += "#define ASSERT_AT(condition, line) do { if (!(condition)) { std::abort(); } } while (false)\n"
    result += constant + "\n" + "\n\n".join(bodies).replace("std::realloc(", "FixtureReallocate(") + "\n"
    if "StoreWebMFrameSize(" in text:
        result += "bool FixtureStoreFrameSize(long nativeSize, int& size) {\n"
        result += "    return StoreWebMFrameSize(nativeSize, size);\n}\n"
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "demux_reference.cpp").write_text(result)
    (args.output / "demux-source.json").write_text(json.dumps({
        "source": str(args.source.resolve()), "sha256": hashlib.sha256(raw).hexdigest(),
        "methods": names, "only_binding_substitution": "std::realloc -> FixtureReallocate",
        "kNanoseconds": constant
    }, indent=2) + "\n")


if __name__ == "__main__":
    main()
