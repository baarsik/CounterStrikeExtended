"""Compile the native geometry producer with each complete retained scrollbar consumer."""
import argparse
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("widget", type=Path)
parser.add_argument("host", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)


def extract(source, signature, closing):
    data = source.read_bytes().replace(b"\r\n", b"\n")
    if data.count(signature) != 1:
        raise ValueError(f"Expected one definition: {signature!r}")
    start = data.index(signature)
    end = data.index(closing, start) + len(closing)
    return data[start:end]


methods = [
    b"void HTML::CalcScrollBars(",
    b"void HTML::BrowserHorizontalScrollBarSizeResponse(",
    b"void HTML::BrowserVerticalScrollBarSizeResponse(",
    b"void HTML::BrowserResize(",
    b"void HTML::UpdateSizeAndScrollBars(",
]
(args.output / "scrollbars.inc").write_bytes(
    b"\n\n".join(extract(args.widget, method, b"\n}") for method in methods) + b"\n"
)
(args.output / "host_geometry.inc").write_bytes(
    extract(args.host, b"    void Geometry(", b"\n    }") + b"\n"
)
