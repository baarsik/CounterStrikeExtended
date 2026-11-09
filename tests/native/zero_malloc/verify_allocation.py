"""Exercise the real fatal branch in a bounded child; never exhaust system memory."""
import resource
import signal
import subprocess
import sys


def no_core():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


normal = subprocess.run([sys.argv[1]], capture_output=True, timeout=3)
assert normal.returncode == 0 and not normal.stderr, (normal.returncode, normal.stderr)
failure = subprocess.run([sys.argv[1], "controlled-null"], capture_output=True, timeout=3, preexec_fn=no_core)
assert failure.returncode == -signal.SIGABRT, (failure.returncode, failure.stderr)
assert b"Mem_ZeroMalloc: allocation failed" in failure.stderr, failure.stderr
assert b"allocation failure returned" not in failure.stderr, failure.stderr
print(normal.stdout.decode().strip())
print("PASS: controlled allocation failure diagnoses and aborts in every configuration")
