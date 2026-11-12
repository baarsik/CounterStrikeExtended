"""Extract complete popup widget methods without altering the legacy source encoding."""
import argparse
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("source", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
source = args.source.read_bytes()
signatures = [
    b"void HTMLComboBoxHost::OnCursorMoved(",
    b"void HTMLComboBoxHost::OnMousePressed(",
    b"void HTMLComboBoxHost::OnMouseReleased(",
    b"void HTMLComboBoxHost::OnMouseDoublePressed(",
    b"void HTML::BrowserSizePopup(",
]
blocks = []
for signature in signatures:
    if source.count(signature) != 1:
        raise ValueError(f"Expected one definition: {signature!r}")
    start = source.index(signature)
    end = source.index(b"\n}", start) + len(b"\n}")
    blocks.append(source[start:end])
args.output.write_bytes(b"\n\n".join(blocks) + b"\n")
