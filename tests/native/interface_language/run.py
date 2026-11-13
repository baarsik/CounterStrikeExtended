#!/usr/bin/env python3
import argparse
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--root', type=Path, default=Path('.'))
parser.add_argument('--evidence', type=Path, required=True)
args = parser.parse_args()
root = args.root.resolve()
out = args.evidence.resolve()
out.mkdir(parents=True, exist_ok=True)
subprocess.run(['python3', str(root / 'tests/native/interface_language/extract.py'), str(root), str(out)], check=True)
subprocess.run(['c++', '-std=c++17', '-g', '-O1', '-Wall', '-Wextra', '-Werror',
                '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-I', str(out),
                '-I', str(root / 'src/common'), str(root / 'tests/native/interface_language/selection_tests.cpp'),
                '-o', str(out / 'selection_tests')], check=True)
subprocess.run([str(out / 'selection_tests')], check=True)
