"""Extract complete production decision bodies; substitute only external seams."""
import argparse
import hashlib
import pathlib
import re

parser = argparse.ArgumentParser()
parser.add_argument("source", type=pathlib.Path)
parser.add_argument("output", type=pathlib.Path)
args = parser.parse_args()
source = args.source.read_text()
header = (args.source.parent / "cbase.h").read_text()


def definition(signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 1
    cursor = opening + 1
    # These two actual bodies contain no braces in string literals or comments.
    while depth:
        depth += (source[cursor] == "{") - (source[cursor] == "}")
        cursor += 1
    return source[start:cursor]


args.output.parent.mkdir(parents=True, exist_ok=True)
constants = [re.search(r"^#define\s+MS_MAX_TARGETS\s+\d+", header, re.M).group(),
             re.search(r"^#define\s+SF_MULTI_INIT\s+\d+", source, re.M).group()]
args.output.with_name("multisource_constants.inc").write_text("\n".join(constants) + "\n")
args.output.write_text("\n\n".join([
    definition("void CMultiSource::Use("),
    definition("BOOL CMultiSource::IsTriggered("),
]) + "\n")
print(f"Extracted complete Use/IsTriggered from {args.source}; "
      f"sha256={hashlib.sha256(args.source.read_bytes()).hexdigest()}")
