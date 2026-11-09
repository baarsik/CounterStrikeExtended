#!/usr/bin/env python3
"""Owned fake-service child processes only; expected nonreturning lease misuse."""
import resource
import signal
import subprocess
import sys

resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
for mode in ("live-destructor", "failed-retirement-destructor"):
    result = subprocess.run([sys.argv[1], mode], capture_output=True, text=True, timeout=10)
    assert result.returncode == -signal.SIGABRT, (mode, result.returncode, result.stdout, result.stderr)
    assert "font selection destroyed before successful retirement" in result.stderr, result.stderr
    print(f"PASS {mode}: diagnostic + SIGABRT; no automatic root callback cleanup")
