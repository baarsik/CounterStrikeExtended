#!/usr/bin/env python3
"""Exercise real console methods with owned PTYs/pipes; never touch the controlling/user terminal."""

import argparse
import errno
import fcntl
import hashlib
import json
import os
from pathlib import Path
import pty
import re
import selectors
import signal
import shlex
import struct
import subprocess
import termios
import time


ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent


def decode_lines(protocol):
    return [bytes.fromhex(item.decode()) for item in re.findall(rb"^LINE ([0-9a-f]*)\r?$", protocol, re.M)]


def pipe_case(binary, case, payload, expected, keep_open=False):
    arguments = [str(binary), case, str(len(expected))]
    if keep_open:
        read_end, write_end = os.pipe()
        try:
            child = subprocess.Popen(arguments, stdin=read_end, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            os.close(read_end)
            read_end = -1
            output, protocol = child.communicate(timeout=5)
        finally:
            if read_end >= 0:
                os.close(read_end)
            os.close(write_end)
    else:
        child = subprocess.run(arguments, input=payload, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=5)
        output, protocol = child.stdout, child.stderr
    assert child.returncode == 0, protocol.decode(errors="replace")
    assert decode_lines(protocol) == expected, (decode_lines(protocol), expected)
    assert b"\x1b" not in output and b"\b" not in output, "redirected output must remain plain"
    if not case.startswith("dup-"):
        assert b"backend message\n" in output, output
    return output, protocol


def terminal_case(binary, case, chunks, expected):
    master, slave = pty.openpty()
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 24, 80, 0, 0))
    original = termios.tcgetattr(slave)
    child = subprocess.Popen([str(binary), case, str(len(expected))], stdin=slave, stdout=slave,
                             stderr=subprocess.PIPE, close_fds=True)
    selector = selectors.DefaultSelector()
    selector.register(master, selectors.EVENT_READ, "output")
    selector.register(child.stderr, selectors.EVENT_READ, "protocol")
    output = bytearray()
    protocol = bytearray()
    sent = False
    terminal_was_configured = False
    deadline = time.monotonic() + 6
    next_chunk = 0
    next_write = None
    try:
        while time.monotonic() < deadline:
            for key, _ in selector.select(0.01):
                try:
                    data = os.read(key.fd, 8192)
                except OSError as error:
                    if error.errno == errno.EIO:
                        data = b""
                    else:
                        raise
                if not data:
                    selector.unregister(key.fileobj)
                elif key.data == "protocol":
                    protocol.extend(data)
                else:
                    output.extend(data)
            if not sent and b"READY\n" in protocol:
                active = termios.tcgetattr(slave)
                assert not (active[3] & (termios.ICANON | termios.ECHO)), active
                terminal_was_configured = True
                sent = True
                next_write = time.monotonic()
            if sent and next_chunk < len(chunks) and time.monotonic() >= next_write:
                os.write(master, chunks[next_chunk])
                next_chunk += 1
                next_write = time.monotonic() + 0.025
            if child.poll() is not None:
                # Drain the separate protocol pipe after exit; a slave remains open for termios comparison.
                protocol.extend(child.stderr.read())
                break
        else:
            raise AssertionError("owned PTY fixture exceeded deadline")
        assert child.returncode == 0, protocol.decode(errors="replace")
        assert terminal_was_configured
        assert decode_lines(protocol) == expected, (decode_lines(protocol), expected)
        restored = termios.tcgetattr(slave)
        assert restored[3] in [original[3], original[3] | termios.PENDIN], "terminal local flags were not restored"
        restored[3] = original[3]
        assert restored == original, "terminal mode was not restored"
        assert b"\x1b]0;HLTV fixture\x07" in output
        assert b"server status" in output
        if case == "hidden":
            assert b"backend message" not in output
        return bytes(output), bytes(protocol)
    finally:
        if child.poll() is None:
            child.kill()
            child.wait()
        selector.close()
        child.stderr.close()
        os.close(master)
        os.close(slave)


def output_failure_case(binary, blocked):
    if not blocked:
        read_end, write_end = os.pipe()
        os.close(read_end)
        try:
            child = subprocess.Popen([str(binary), "closed-output", "0"], stdin=subprocess.DEVNULL,
                                     stdout=write_end, stderr=subprocess.PIPE)
        finally:
            os.close(write_end)
        _, protocol = child.communicate(timeout=5)
        assert child.returncode == 0, protocol.decode(errors="replace")
        return b"", protocol
    child = subprocess.Popen([str(binary), "blocked-output", "0"], stdin=subprocess.DEVNULL,
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    protocol = bytearray()
    try:
        deadline = time.monotonic() + 4
        while b"READY\n" not in protocol and time.monotonic() < deadline:
            ready, _, _ = __import__("select").select([child.stderr], [], [], 0.05)
            if ready:
                protocol.extend(os.read(child.stderr.fileno(), 8192))
            assert child.poll() is None, protocol
        assert b"READY\n" in protocol
        time.sleep(0.05)
        assert child.poll() is None, "output must be blocked before the signal is sent"
        child.send_signal(signal.SIGTERM)
        child.wait(timeout=3)  # Do not drain stdout until the handler/flag has actually unblocked the writer.
        stdout, tail = child.communicate(timeout=1)
        protocol.extend(tail)
        assert child.returncode == 0 and b"DONE\n" in protocol, protocol.decode(errors="replace")
        return stdout, bytes(protocol)
    finally:
        if child.poll() is None:
            child.kill()
            child.wait()
        child.stdout.close()
        child.stderr.close()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--commands", required=True, type=Path, help="Frozen actual TU RED recipe inventory")
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    entry = next(item for item in json.loads(args.commands.read_text()) if item["label"] == "TextConsole-release")
    original = entry["args"]
    prefix = original[:original.index("-o")]
    profiles = [("arm64-release", "arm64", ["-O2", "-DNDEBUG"]), ("arm64-debug", "arm64", ["-O0", "-g"]),
                ("arm64-sanitizers", "arm64", ["-O1", "-g", "-fsanitize=address,undefined",
                                               "-fno-sanitize-recover=all", "-fno-omit-frame-pointer"]),
                ("x86_64-release", "x86_64", ["-O2", "-DNDEBUG"]),
                ("x86_64-debug", "x86_64", ["-O0", "-g"])]
    receipts = []
    for name, architecture, flags in profiles:
        directory = output / name
        directory.mkdir(exist_ok=True)
        common = [value for value in prefix if not value.startswith("-std=") and value not in ["-O2", "-O0"]]
        common[common.index("-arch") + 1] = architecture
        common += ["-std=c++14"] + flags
        objects = []
        for label, source, bindings in [
                ("base", ROOT / "src/HLTV/Console/TextConsole.cpp", []),
                ("backend", ROOT / "src/HLTV/Console/TextConsolePosix.cpp",
                 ["-Ddup=CseConsoleDuplicate", "-Dread=CseConsoleRead", "-Dwrite=CseConsoleWrite"]),
                ("helpers", ROOT / "src/HLTV/Console/PosixSystem.cpp", []),
                ("list", ROOT / "src/common/ObjectList.cpp", []),
                ("fixture", HERE / "backend_tests.cpp", [])]:
            obj = directory / (label + ".o")
            command = common + bindings + ["-c", str(source), "-o", str(obj)]
            result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            (directory / (label + "-compile.log")).write_text(shlex.join(command) + "\n\n" + result.stdout)
            assert result.returncode == 0, result.stdout
            objects.append(str(obj))
        binary = directory / "backend"
        command = ["/usr/bin/clang++", "-arch", architecture] + flags + objects + ["-o", str(binary)]
        result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (directory / "link.log").write_text(shlex.join(command) + "\n\n" + result.stdout)
        assert result.returncode == 0, result.stdout
        history_object = directory / "history.o"
        command = common + ["-c", str(HERE / "history_tests.cpp"), "-o", str(history_object)]
        result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (directory / "history-compile.log").write_text(shlex.join(command) + "\n\n" + result.stdout)
        assert result.returncode == 0, result.stdout
        history_binary = directory / "history"
        command = ["/usr/bin/clang++", "-arch", architecture] + flags
        command += [str(history_object), str(directory / "base.o"), str(directory / "list.o"), "-o", str(history_binary)]
        result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        (directory / "history-link.log").write_text(shlex.join(command) + "\n\n" + result.stdout)
        assert result.returncode == 0, result.stdout
        for count in [29, 30, 31, 59, 60]:
            result = subprocess.run([str(history_binary), str(count)], capture_output=True, text=True, timeout=5)
            (directory / ("history-" + str(count) + ".log")).write_text(result.stdout + result.stderr)
            assert result.returncode == 0, result.stdout + result.stderr
            receipts.append({"profile": name, "case": "history-" + str(count), "passed": True})
        cases = [
            ("pipe-eof", "eof", b"one\ntwo", [b"one", b"two"]),
            ("pipe-queue", "lines", b"first\r\nsecond\nthird\r", [b"first", b"second", b"third"]),
            ("pipe-edit", "lines", b"ab\x1b[DZ\x7fX\n", [b"aXb"]),
            ("pipe-history", "lines", b"first\nsecond\n\x1b[A\n", [b"first", b"second", b"second"]),
            ("pipe-completion", "completion", b"sta\t\n", [b"status "]),
            ("pipe-interrupted", "interrupted", b"echo\n", [b"echo"]),
            ("pipe-hidden", "hidden", b"quit\n", [b"quit"]),
            ("pipe-dup-input-error", "dup-input-error", b"", []),
            ("pipe-dup-output-error", "dup-output-error", b"", []),
            ("pipe-no-input", "no-input", b"", []),
            ("pipe-read-error", "read-error", b"bad\n", []),
        ]
        for label, case, data, expected in cases:
            stdout, protocol = pipe_case(binary, case, data, expected, keep_open=case == "no-input")
            (directory / (label + ".stdout")).write_bytes(stdout)
            (directory / (label + ".protocol")).write_bytes(protocol)
            receipts.append({"profile": name, "case": label, "passed": True})
        for label, case, chunks, expected in [
                ("tty-fragmented-escape", "lines", [b"ab\x1b", b"[D", b"X\r\n"], [b"aXb"]),
                ("tty-history", "lines", [b"first\rsecond\r\x1b[A\n"], [b"first", b"second", b"second"]),
                ("tty-completion", "completion", [b"sta\t\r"], [b"status "]),
                ("tty-hidden", "hidden", [b"quit\r"], [b"quit"]),
                ("tty-idle", "no-input", [], []),
                ("tty-interrupted", "interrupted", [b"echo\r"], [b"echo"])]:
            stdout, protocol = terminal_case(binary, case, chunks, expected)
            (directory / (label + ".stdout")).write_bytes(stdout)
            (directory / (label + ".protocol")).write_bytes(protocol)
            receipts.append({"profile": name, "case": label, "passed": True})
        for blocked in [False, True]:
            label = "output-blocked-signal" if blocked else "output-closed"
            stdout, protocol = output_failure_case(binary, blocked)
            (directory / (label + ".stdout")).write_bytes(stdout)
            (directory / (label + ".protocol")).write_bytes(protocol)
            receipts.append({"profile": name, "case": label, "passed": True})
        print(name, "19 backend + 5 actual-base history cases passed", flush=True)
    (output / "receipt.json").write_text(json.dumps({"cases": receipts, "command_inventory_sha256":
        hashlib.sha256(args.commands.read_bytes()).hexdigest()}, indent=2) + "\n")


if __name__ == "__main__":
    main()
