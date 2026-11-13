import pathlib
import subprocess
import sys
from actual_arguments import definitions

root = pathlib.Path(__file__).resolve().parents[3]
out = pathlib.Path(sys.argv[1]).resolve()
out.mkdir(parents=True, exist_ok=True)
(out / 'actual_arguments.inc').write_text(definitions(root))
command = ['clang++', sys.argv[2], '-std=c++17', '-UNDEBUG', '-Wall', '-Wextra', '-Werror',
           '-ffp-model=precise', '-ffp-contract=off', '-Wno-overriding-option',
           '-I' + str(root / 'src'), '-I' + str(out),
           str(root / 'tests/native/dedicated_host/engine_arguments.cpp'), '-o', str(out / 'engine_arguments')]
subprocess.run(command, check=True)
subprocess.run([str(out / 'engine_arguments')], check=True)
