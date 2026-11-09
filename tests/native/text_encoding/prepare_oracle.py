import argparse
import gzip
import hashlib
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--input', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
data = gzip.decompress(args.input.read_bytes())
assert hashlib.sha256(data).hexdigest() == '14dbac63e7d3810e9b14a2517d475c9defd7417c0de32b045d58d371d311de2c'
args.output.write_bytes(data)
