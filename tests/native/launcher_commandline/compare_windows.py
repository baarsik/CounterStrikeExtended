"""Compare real parser results against the preserved source in separate processes."""

import subprocess
import sys
import tempfile

for scenario in ("basic", "mutators", "quoted", "response", "whitespace", "limits"):
    outputs = []
    for parser in sys.argv[1:]:
        with tempfile.TemporaryDirectory(prefix="cse-commandline-") as directory:
            result = subprocess.run([parser, scenario], cwd=directory, text=True,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            if result.returncode:
                print(result.stdout)
                sys.exit(result.returncode)
            outputs.append(result.stdout)
    if outputs[0] != outputs[1]:
        print(f"Windows parser behavior changed: {scenario}")
        print(outputs)
        sys.exit(1)
print("launcher_windows_reference_comparison_passed=6")
