"""Actual session0 Windows ConPTY controls; no production/runtime processes."""
import argparse
import ctypes as C
from ctypes import wintypes as W
import json
import os
from pathlib import Path
import re
import sys
import threading
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from server_lifecycle_probe import ConsoleProcess, STARTF_USESTDHANDLES, await_text

EVIDENCE = None
AUDIT = []


class PROCESSENTRY(C.Structure):
    _fields_ = [('size', W.DWORD), ('usage', W.DWORD), ('pid', W.DWORD), ('heap', C.c_size_t),
                ('module', W.DWORD), ('threads', W.DWORD), ('parent', W.DWORD), ('priority', W.LONG),
                ('flags', W.DWORD), ('name', W.WCHAR * 260)]


def own_children():
    api = C.WinDLL('kernel32', use_last_error=True)
    api.CreateToolhelp32Snapshot.argtypes = [W.DWORD, W.DWORD]
    api.CreateToolhelp32Snapshot.restype = W.HANDLE
    api.Process32FirstW.argtypes = [W.HANDLE, C.POINTER(PROCESSENTRY)]
    api.Process32FirstW.restype = W.BOOL
    api.Process32NextW.argtypes = [W.HANDLE, C.POINTER(PROCESSENTRY)]
    api.Process32NextW.restype = W.BOOL
    api.CloseHandle.argtypes = [W.HANDLE]
    snapshot = api.CreateToolhelp32Snapshot(2, 0)
    if snapshot == C.c_void_p(-1).value:
        raise C.WinError(C.get_last_error())
    rows = []
    entry = PROCESSENTRY()
    entry.size = C.sizeof(entry)
    try:
        more = api.Process32FirstW(snapshot, C.byref(entry))
        while more:
            rows.append({'pid': entry.pid, 'parent': entry.parent, 'name': entry.name})
            more = api.Process32NextW(snapshot, C.byref(entry))
    finally:
        api.CloseHandle(snapshot)
    parents = {os.getpid()}
    selected = []
    while True:
        new = [row for row in rows if row['parent'] in parents and row['pid'] not in parents]
        if not new:
            return selected
        selected.extend(new)
        parents.update(row['pid'] for row in new)


def windows_for(pids):
    api = C.WinDLL('user32', use_last_error=True)
    callback_type = C.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)
    api.GetForegroundWindow.restype = W.HWND
    api.IsWindowVisible.argtypes = [W.HWND]
    api.IsWindowVisible.restype = W.BOOL
    api.GetWindowThreadProcessId.argtypes = [W.HWND, C.POINTER(W.DWORD)]
    api.EnumWindows.argtypes = [callback_type, W.LPARAM]
    result = []

    @callback_type
    def inspect(window, _):
        pid = W.DWORD()
        api.GetWindowThreadProcessId(window, C.byref(pid))
        if pid.value in pids and api.IsWindowVisible(window):
            result.append({'pid': pid.value, 'window': window})
        return True

    if not api.EnumWindows(inspect, 0):
        raise C.WinError(C.get_last_error())
    return {'foreground': api.GetForegroundWindow(), 'visible_owned_windows': result}


class TransportTests(unittest.TestCase):
    def setUp(self):
        self.before = own_children()
        self.foreground = windows_for(set())['foreground']
        self.processes = []
        self.tracked = []
        self.case = self.id().split('.')[-1]
        self.detail = {'case': self.case}

    def start(self, command):
        process = ConsoleProcess(command, EVIDENCE, EVIDENCE / (self.case + '.raw'))
        self.processes.append(process)
        children = own_children()
        self.detail['live_children'] = children
        self.detail['session'] = process.session_id
        self.assertEqual(process.session_id, 0, 'fixture must remain in noninteractive session0')
        view = windows_for({row['pid'] for row in children} | {process.pid})
        self.detail['live_window_audit'] = view
        self.assertFalse(view['visible_owned_windows'])
        self.assertEqual(view['foreground'], self.foreground)
        api = process.api
        api.OpenProcess.argtypes = [W.DWORD, W.BOOL, W.DWORD]
        api.OpenProcess.restype = W.HANDLE
        for row in children:
            handle = api.OpenProcess(0x00101000, False, row['pid'])
            if handle:
                self.tracked.append((api, handle, row))
        return process

    def child(self, mode):
        receipt = EVIDENCE / (self.case + '-child.json')
        process = self.start([sys.executable, ROOT / 'tests/console_transport_child.py',
                              '--receipt', receipt, '--mode', mode])
        return process, receipt

    def tearDown(self):
        try:
            for process in reversed(self.processes):
                process.close()
                self.assertTrue(process.closed)
                self.assertFalse(process.reader.is_alive())
            reaped = []
            for api, handle, row in self.tracked:
                try:
                    self.assertEqual(api.WaitForSingleObject(handle, 3000), 0, 'owned child/helper remained alive')
                    reaped.append(row)
                finally:
                    api.CloseHandle(handle)
            self.detail['tracked_reaped'] = reaped
            deadline = time.monotonic() + 3
            while own_children() != self.before and time.monotonic() < deadline:
                time.sleep(0.05)
            self.detail['children_after'] = own_children()
            self.assertEqual(self.detail['children_after'], self.before)
            self.assertEqual(windows_for(set())['foreground'], self.foreground)
        finally:
            AUDIT.append(self.detail)

    def test_cmd_echo_is_captured(self):
        process = self.start([r'C:\Windows\System32\cmd.exe', '/d', '/q', '/c', 'echo CSE_CMD_ECHO'])
        self.assertEqual(process.wait(10), 0)
        process.close()
        self.assertIn('CSE_CMD_ECHO', process.text())
        self.assertFalse(process.forced)
        process.close()
        self.assertEqual(process.poll(), 0)
        with self.assertRaisesRegex(RuntimeError, 'closed'):
            process.send(['echo INVALID_AFTER_CLOSE'])

    def test_child_console_handles(self):
        process, receipt = self.child('probe')
        self.assertEqual(process.wait(10), 0)
        process.close()
        result = json.loads(receipt.read_text())
        self.assertEqual(result['startup_flags'], STARTF_USESTDHANDLES)
        for handle in result['handles'].values():
            self.assertTrue(handle['console_mode_ok'])
            self.assertEqual(handle['file_type'], 2)
        self.assertIn('CSE_NATIVE_CONSOLE_PROBE', process.text())

    def test_cmd_delayed_typed_command(self):
        process = self.start([r'C:\Windows\System32\cmd.exe', '/d', '/q'])
        time.sleep(0.35)
        process.send(['echo CSE_DELAYED_CMD'])
        # ConPTY positions the next prompt with a cursor sequence, not LF.
        # A line starting with the token excludes the echoed 'echo TOKEN' input.
        await_text(process, lambda text: re.search(r'(?m)^CSE_DELAYED_CMD', text) is not None,
                   'cmd delayed echo', timeout=5)
        process.send(['exit /b 0'])
        self.assertEqual(process.wait(5), 0)

    def test_retained_input_record_api(self):
        process, receipt = self.child('events')
        await_text(process, lambda text: 'CSE_INPUT_RECORDS_READY' in text, 'input-record readiness', timeout=5)
        time.sleep(0.35)
        process.send(['CSE_DELAYED_INPUT'])
        self.assertEqual(process.wait(5), 0)
        process.close()
        self.assertIn('CSE_INPUT_RECORDS_RECEIVED:CSE_DELAYED_INPUT', process.text())
        result = json.loads(receipt.read_text())
        self.assertEqual(result['received_line'], 'CSE_DELAYED_INPUT')
        self.assertEqual(result['return_virtual_key'], 13)

    def test_large_output_drains_and_exits(self):
        process, _ = self.child('large')
        self.assertEqual(process.wait(10), 0)
        process.close()
        text = process.text()
        found = set(re.findall(r'CSE_LINE_(\d{4}):' + 'x' * 64, text))
        self.assertEqual(found, {f'{index:04d}' for index in range(512)})
        self.assertIn('CSE_LARGE_OUTPUT_END', text)
        self.assertGreaterEqual(process.log.stat().st_size, 16 * 1024)
        self.detail['raw_output_bytes'] = process.log.stat().st_size
        self.detail['complete_payload_lines'] = len(found)

    def test_cooperative_close_uses_real_console_input(self):
        process, receipt = self.child('events')
        await_text(process, lambda text: 'CSE_INPUT_RECORDS_READY' in text, 'cooperative input readiness', timeout=5)
        process.close()
        self.assertEqual(process.exit_code, 0)
        self.assertFalse(process.forced)
        self.assertEqual(json.loads(receipt.read_text())['received_line'], 'quit')

    def test_timeout_forces_only_owned_child_and_joins_reader(self):
        process, receipt = self.child('sleep')
        deadline = time.monotonic() + 5
        while not receipt.exists() and time.monotonic() < deadline:
            time.sleep(0.01)
        self.assertTrue(receipt.exists())
        with self.assertRaises(TimeoutError):
            process.wait(0.1)
        started = time.monotonic()
        process.close()
        self.assertTrue(process.forced)
        self.assertEqual(process.exit_code, 1)
        self.assertLess(time.monotonic() - started, 8)

    def test_failed_creation_rolls_back_pseudoconsole(self):
        before_threads = {thread.ident for thread in threading.enumerate()}
        with self.assertRaises(OSError):
            ConsoleProcess([EVIDENCE / 'missing-owned-command.exe'], EVIDENCE, EVIDENCE / 'failed-create.raw')
        self.assertEqual({thread.ident for thread in threading.enumerate()}, before_threads)


def main():
    global EVIDENCE
    if os.name != 'nt':
        raise SystemExit('Real Windows controls required')
    parser = argparse.ArgumentParser()
    parser.add_argument('--evidence', type=Path, required=True)
    args = parser.parse_args()
    EVIDENCE = args.evidence
    EVIDENCE.mkdir(parents=True, exist_ok=False)
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(TransportTests)
    result = unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(suite)
    (EVIDENCE / 'result.json').write_text(json.dumps({'success': result.wasSuccessful(), 'tests': result.testsRun,
                                                  'failures': len(result.failures), 'errors': len(result.errors),
                                                  'audit': AUDIT}, indent=2) + '\n')
    return 0 if result.wasSuccessful() else 1


if __name__ == '__main__':
    raise SystemExit(main())
