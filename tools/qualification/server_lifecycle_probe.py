"""Owned Windows console transports and bounded local dedicated/HLTV lifecycle.

Retained GUI launchers allocate their own console; RetainedConsoleProcess attaches
only to that owned console. Console images can use the isolated ConPTY transport.
The readers consume ReadConsoleInput records rather than redirected stdin. API setup:
https://learn.microsoft.com/en-us/windows/console/creating-a-pseudoconsole-session
No production injection, window messages, keyboard automation or public server.
"""
import argparse
import ctypes as C
from ctypes import wintypes as W
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import threading
import time

CSI = re.compile(r'\x1b\[[0-?]*[ -/]*[@-~]')
OSC = re.compile(r'\x1b\].*?(?:\x07|\x1b\\)', re.S)
EXTENDED_STARTUPINFO_PRESENT = 0x00080000
STARTF_USESTDHANDLES = 0x00000100
PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE = 0x00020016


def plain(data):
    return CSI.sub('', OSC.sub('', data.decode('utf-8', errors='replace'))).replace('\r', '\n')


def digest(path):
    with open(path, 'rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def save(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')


class COORD(C.Structure):
    _fields_ = [('X', W.SHORT), ('Y', W.SHORT)]


class STARTUPINFO(C.Structure):
    _fields_ = [('cb', W.DWORD), ('lpReserved', W.LPWSTR), ('lpDesktop', W.LPWSTR), ('lpTitle', W.LPWSTR),
                ('dwX', W.DWORD), ('dwY', W.DWORD), ('dwXSize', W.DWORD), ('dwYSize', W.DWORD),
                ('dwXCountChars', W.DWORD), ('dwYCountChars', W.DWORD), ('dwFillAttribute', W.DWORD),
                ('dwFlags', W.DWORD), ('wShowWindow', W.WORD), ('cbReserved2', W.WORD),
                ('lpReserved2', C.c_void_p), ('hStdInput', W.HANDLE), ('hStdOutput', W.HANDLE), ('hStdError', W.HANDLE)]


class STARTUPINFOEX(C.Structure):
    _fields_ = [('StartupInfo', STARTUPINFO), ('lpAttributeList', C.c_void_p)]


class PROCESSINFO(C.Structure):
    _fields_ = [('hProcess', W.HANDLE), ('hThread', W.HANDLE), ('dwProcessId', W.DWORD), ('dwThreadId', W.DWORD)]


class CONSOLEINFO(C.Structure):
    class RECT(C.Structure):
        _fields_ = [('Left', W.SHORT), ('Top', W.SHORT), ('Right', W.SHORT), ('Bottom', W.SHORT)]

    _fields_ = [('size', COORD), ('cursor', COORD), ('attributes', W.WORD), ('window', RECT), ('maximum', COORD)]


class KEYCHAR(C.Union):
    _fields_ = [('UnicodeChar', W.WCHAR), ('AsciiChar', C.c_char)]


class KEYEVENT(C.Structure):
    _fields_ = [('down', W.BOOL), ('repeat', W.WORD), ('virtual_key', W.WORD),
                ('scan', W.WORD), ('character', KEYCHAR), ('control', W.DWORD)]


class EVENTDATA(C.Union):
    _fields_ = [('key', KEYEVENT), ('other', C.c_byte * 16)]


class INPUTRECORD(C.Structure):
    _fields_ = [('kind', W.WORD), ('event', EVENTDATA)]


class ConsoleProcess:
    def __init__(self, command, cwd, log):
        self.api = C.WinDLL('kernel32', use_last_error=True)
        signatures = {
            'CreatePipe': ([C.POINTER(W.HANDLE), C.POINTER(W.HANDLE), C.c_void_p, W.DWORD], W.BOOL),
            'CreatePseudoConsole': ([COORD, W.HANDLE, W.HANDLE, W.DWORD, C.POINTER(W.HANDLE)], C.c_long),
            'ClosePseudoConsole': ([W.HANDLE], None),
            'CloseHandle': ([W.HANDLE], W.BOOL),
            'InitializeProcThreadAttributeList': ([C.c_void_p, W.DWORD, W.DWORD, C.POINTER(C.c_size_t)], W.BOOL),
            'UpdateProcThreadAttribute': ([C.c_void_p, W.DWORD, C.c_size_t, C.c_void_p, C.c_size_t, C.c_void_p, C.c_void_p], W.BOOL),
            'DeleteProcThreadAttributeList': ([C.c_void_p], None),
            'CreateProcessW': ([W.LPCWSTR, W.LPWSTR, C.c_void_p, C.c_void_p, W.BOOL, W.DWORD, C.c_void_p, W.LPCWSTR, C.POINTER(STARTUPINFOEX), C.POINTER(PROCESSINFO)], W.BOOL),
            'ReadFile': ([W.HANDLE, C.c_void_p, W.DWORD, C.POINTER(W.DWORD), C.c_void_p], W.BOOL),
            'WriteFile': ([W.HANDLE, C.c_void_p, W.DWORD, C.POINTER(W.DWORD), C.c_void_p], W.BOOL),
            'WaitForSingleObject': ([W.HANDLE, W.DWORD], W.DWORD),
            'GetExitCodeProcess': ([W.HANDLE, C.POINTER(W.DWORD)], W.BOOL),
            'TerminateProcess': ([W.HANDLE, W.UINT], W.BOOL),
            'ProcessIdToSessionId': ([W.DWORD, C.POINTER(W.DWORD)], W.BOOL),
        }
        for name, (arguments, result) in signatures.items():
            function = getattr(self.api, name)
            function.argtypes = arguments
            function.restype = result
        self.pc = W.HANDLE()
        self.input = W.HANDLE()
        self.output = W.HANDLE()
        input_read = W.HANDLE()
        output_write = W.HANDLE()
        self.info = PROCESSINFO()
        self.reader = None
        self.data = bytearray()
        self.lock = threading.Lock()
        self.log = log
        self.closed = False
        self.forced = False
        self.exit_code = None
        self.reader_error = None
        attributes = None
        attributes_initialized = False
        try:
            self.check(self.api.CreatePipe(C.byref(input_read), C.byref(self.input), None, 0), 'CreatePipe input')
            self.check(self.api.CreatePipe(C.byref(self.output), C.byref(output_write), None, 0), 'CreatePipe output')
            result = self.api.CreatePseudoConsole(COORD(160, 50), input_read, output_write, 0, C.byref(self.pc))
            if result < 0:
                raise OSError(f'CreatePseudoConsole HRESULT 0x{result & 0xffffffff:08x}')
            self.reader = threading.Thread(target=self.read_output, name='owned-conpty-output')
            self.reader.start()
            size = C.c_size_t()
            self.api.InitializeProcThreadAttributeList(None, 1, 0, C.byref(size))
            attributes = C.create_string_buffer(size.value)
            self.check(self.api.InitializeProcThreadAttributeList(attributes, 1, 0, C.byref(size)),
                       'InitializeProcThreadAttributeList')
            attributes_initialized = True
            startup = STARTUPINFOEX()
            startup.StartupInfo.cb = C.sizeof(STARTUPINFOEX)
            # Explicit NULL standard handles prevent inherited parent pipes from
            # bypassing ConPTY. Console attachment fills them with CONIN$/CONOUT$.
            # Keep handle inheritance disabled; CREATE_NO_WINDOW removes the
            # console handle needed by the retained ReadConsoleInput readers.
            startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES
            startup.lpAttributeList = C.cast(attributes, C.c_void_p)
            self.check(self.api.UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                                         self.pc, C.sizeof(W.HANDLE), None, None),
                       'UpdateProcThreadAttribute')
            mutable_command = C.create_unicode_buffer(subprocess.list2cmdline([str(item) for item in command]))
            self.check(self.api.CreateProcessW(str(command[0]), mutable_command, None, None, False,
                                              EXTENDED_STARTUPINFO_PRESENT,
                                              None, str(cwd), C.byref(startup), C.byref(self.info)), 'CreateProcessW')
            self.api.CloseHandle(self.info.hThread)
            self.info.hThread = None
            session = W.DWORD()
            self.check(self.api.ProcessIdToSessionId(self.pid, C.byref(session)), 'ProcessIdToSessionId')
            self.session_id = session.value
        except BaseException:
            if input_read:
                self.api.CloseHandle(input_read)
                input_read = None
            if output_write:
                self.api.CloseHandle(output_write)
                output_write = None
            self.close()
            raise
        finally:
            if attributes_initialized:
                self.api.DeleteProcThreadAttributeList(attributes)
            if input_read:
                self.api.CloseHandle(input_read)
            if output_write:
                self.api.CloseHandle(output_write)

    def check(self, result, operation):
        if not result:
            raise C.WinError(C.get_last_error(), operation)

    @property
    def pid(self):
        return self.info.dwProcessId

    def read_output(self):
        with self.log.open('wb') as stream:
            while True:
                buffer = C.create_string_buffer(8192)
                count = W.DWORD()
                if not self.api.ReadFile(self.output, buffer, len(buffer), C.byref(count), None):
                    error = C.get_last_error()
                    if error not in (109, 232):  # Broken/closed anonymous pipe.
                        self.reader_error = error
                    break
                if not count.value:
                    break
                block = buffer.raw[:count.value]
                with self.lock:
                    self.data.extend(block)
                stream.write(block)
                stream.flush()

    def text(self):
        with self.lock:
            return plain(bytes(self.data))

    def send(self, commands):
        if self.closed:
            raise RuntimeError('cannot write to a closed owned console')
        data = ('\r'.join(commands) + '\r').encode('ascii')
        count = W.DWORD()
        self.check(self.api.WriteFile(self.input, data, len(data), C.byref(count), None), 'ConPTY WriteFile')
        if count.value != len(data):
            raise OSError('partial ConPTY command write')

    def poll(self):
        if self.closed:
            return self.exit_code
        if self.api.WaitForSingleObject(self.info.hProcess, 0) == 258:
            return None
        code = W.DWORD()
        self.check(self.api.GetExitCodeProcess(self.info.hProcess, C.byref(code)), 'GetExitCodeProcess')
        return code.value

    def wait(self, seconds):
        deadline = time.monotonic() + seconds
        while self.poll() is None:
            if time.monotonic() > deadline:
                raise TimeoutError(f'owned process {self.pid} did not exit')
            time.sleep(0.05)
        return self.poll()

    def close(self):
        if self.closed:
            return
        if self.info.hProcess and self.poll() is None:
            try:
                self.send(['quit'])
                self.wait(5)
            except (OSError, TimeoutError):
                self.forced = True
                self.check(self.api.TerminateProcess(self.info.hProcess, 1), 'Terminate owned failed process')
                self.wait(5)
        if self.info.hProcess:
            self.exit_code = self.poll()
        if self.pc:
            self.api.ClosePseudoConsole(self.pc)
            self.pc = None
        if self.input:
            self.api.CloseHandle(self.input)
            self.input = None
        if self.reader:
            self.reader.join(5)
            if self.reader.is_alive():
                raise RuntimeError('ConPTY output reader did not finish cooperatively')
        if self.output:
            self.api.CloseHandle(self.output)
            self.output = None
        if self.info.hProcess:
            self.api.CloseHandle(self.info.hProcess)
            self.info.hProcess = None
        self.closed = True
        if self.reader_error:
            raise OSError(self.reader_error, 'ConPTY output reader failed')


class RetainedConsoleProcess(ConsoleProcess):
    """Control the real console allocated by an owned GUI-subsystem launcher.

    These launchers call AllocConsole themselves. Their native console remains
    visible for developer acceptance. Do not launch this mode in a shared desktop
    lane. No conproc worker, injected code or external window input is used.
    """
    console_lock = threading.RLock()
    maximum_snapshot_rows = 2048

    def __init__(self, command, cwd, log, file_logs=()):
        self.api = C.WinDLL('kernel32', use_last_error=True)
        signatures = {
            'CreateProcessW': ([W.LPCWSTR, W.LPWSTR, C.c_void_p, C.c_void_p, W.BOOL, W.DWORD,
                               C.c_void_p, W.LPCWSTR, C.POINTER(STARTUPINFOEX), C.POINTER(PROCESSINFO)], W.BOOL),
            'CloseHandle': ([W.HANDLE], W.BOOL),
            'WaitForSingleObject': ([W.HANDLE, W.DWORD], W.DWORD),
            'GetExitCodeProcess': ([W.HANDLE, C.POINTER(W.DWORD)], W.BOOL),
            'TerminateProcess': ([W.HANDLE, W.UINT], W.BOOL),
            'ProcessIdToSessionId': ([W.DWORD, C.POINTER(W.DWORD)], W.BOOL),
            'FreeConsole': ([], W.BOOL),
            'AttachConsole': ([W.DWORD], W.BOOL),
            'GetConsoleWindow': ([], W.HWND),
            'CreateFileW': ([W.LPCWSTR, W.DWORD, W.DWORD, C.c_void_p, W.DWORD, W.DWORD, W.HANDLE], W.HANDLE),
            'GetConsoleMode': ([W.HANDLE, C.POINTER(W.DWORD)], W.BOOL),
            'GetNumberOfConsoleInputEvents': ([W.HANDLE, C.POINTER(W.DWORD)], W.BOOL),
            'GetConsoleScreenBufferInfo': ([W.HANDLE, C.POINTER(CONSOLEINFO)], W.BOOL),
            'ReadConsoleOutputCharacterW': ([W.HANDLE, W.LPWSTR, W.DWORD, COORD, C.POINTER(W.DWORD)], W.BOOL),
            'WriteConsoleInputA': ([W.HANDLE, C.POINTER(INPUTRECORD), W.DWORD, C.POINTER(W.DWORD)], W.BOOL),
        }
        for name, (arguments, result) in signatures.items():
            function = getattr(self.api, name)
            function.argtypes = arguments
            function.restype = result
        self.info = PROCESSINFO()
        self.closed = False
        self.forced = False
        self.exit_code = None
        self.session_id = None
        self.log = log
        self.file_logs = [Path(path) for path in file_logs]
        self.snapshot = ''
        self.console_observation = {}
        startup = STARTUPINFOEX()
        # Deliberately no ConPTY attribute and no STARTF_USESTDHANDLES. The GUI
        # child must allocate its console and initialize its own standard handles.
        startup.StartupInfo.cb = C.sizeof(STARTUPINFO)
        mutable = C.create_unicode_buffer(subprocess.list2cmdline([str(item) for item in command]))
        self.check(self.api.CreateProcessW(str(command[0]), mutable, None, None, False, 0,
                                          None, str(cwd), C.byref(startup), C.byref(self.info)), 'Create GUI console owner')
        self.api.CloseHandle(self.info.hThread)
        self.info.hThread = None
        session = W.DWORD()
        self.check(self.api.ProcessIdToSessionId(self.pid, C.byref(session)), 'ProcessIdToSessionId')
        self.session_id = session.value

    def attach(self):
        self.api.FreeConsole()
        if not self.api.AttachConsole(self.pid):
            raise C.WinError(C.get_last_error(), 'Attach owned console')
        handles = []
        try:
            for name in ('CONIN$', 'CONOUT$'):
                handle = self.api.CreateFileW(name, 0xc0000000, 3, None, 3, 0, None)
                if handle == C.c_void_p(-1).value:
                    raise C.WinError(C.get_last_error(), 'Open owned ' + name)
                handles.append(handle)
            modes = []
            for handle in handles:
                mode = W.DWORD()
                self.check(self.api.GetConsoleMode(handle, C.byref(mode)), 'Owned console mode')
                modes.append(mode.value)
            self.console_observation = {'window': self.api.GetConsoleWindow(), 'input_mode': modes[0],
                                        'output_mode': modes[1], 'owner_pid': self.pid, 'session': self.session_id}
            return handles
        except BaseException:
            self.detach(handles)
            raise

    def detach(self, handles):
        for handle in handles:
            self.api.CloseHandle(handle)
        self.api.FreeConsole()

    def text(self):
        if not self.closed and self.poll() is None:
            with self.console_lock:
                handles = []
                try:
                    handles = self.attach()
                    info = CONSOLEINFO()
                    self.check(self.api.GetConsoleScreenBufferInfo(handles[1], C.byref(info)), 'Console buffer info')
                    rows = min(info.cursor.Y + 1, self.maximum_snapshot_rows)
                    first = max(0, info.cursor.Y + 1 - rows)
                    count = rows * info.size.X
                    if count > 0:
                        output = C.create_unicode_buffer(count + 1)
                        read = W.DWORD()
                        self.check(self.api.ReadConsoleOutputCharacterW(
                            handles[1], output, count, COORD(0, first), C.byref(read)), 'Read owned console buffer')
                        value = output[:read.value]
                        self.snapshot = '\n'.join(value[index:index + info.size.X].rstrip()
                                                  for index in range(0, len(value), info.size.X))
                        self.log.write_text(self.snapshot)
                except OSError:
                    # Console allocation/closure races are represented by the
                    # last snapshot plus durable application logs.
                    pass
                finally:
                    self.detach(handles)
        logs = []
        for path in self.file_logs:
            if path.is_file():
                try:
                    logs.append(path.read_text(encoding='utf-8', errors='replace'))
                except OSError:
                    pass
        return '\n'.join(logs + [self.snapshot])

    def send(self, commands):
        if self.closed:
            raise RuntimeError('cannot write to a closed owned console')
        # The retained GetLine returns at the first Return and drops the rest of
        # that ReadConsoleInput batch. Observe its queue consumption before each
        # later command; a fixed delay alone would depend on engine frame timing.
        maximum_command_characters = 254  # CTextConsole reserves two of its 256 bytes.
        commands = list(commands)
        for command in commands:
            if not command or len(command) > maximum_command_characters or any(
                    not ' ' <= character <= '~' for character in command):
                raise ValueError('owned console commands require bounded printable ASCII')
        with self.console_lock:
            for command_index, command in enumerate(commands):
                text = command + '\r'
                records = (INPUTRECORD * (len(text) * 2))()
                for index, character in enumerate(text):
                    for up in range(2):
                        record = records[index * 2 + up]
                        record.kind = 1
                        record.event.key.down = not up
                        record.event.key.repeat = 1
                        record.event.key.virtual_key = 13 if character == '\r' else ord(character.upper())
                        record.event.key.character.UnicodeChar = character
                deadline = time.monotonic() + 5
                handles = []
                try:
                    while not handles:
                        try:
                            handles = self.attach()
                        except OSError:
                            if self.poll() is not None or time.monotonic() > deadline:
                                raise
                            time.sleep(0.01)
                    written = W.DWORD()
                    self.check(self.api.WriteConsoleInputA(handles[0], records, len(records), C.byref(written)),
                               'Write owned console input')
                    if written.value != len(records):
                        raise OSError('partial owned console input write')
                    while True:
                        pending = W.DWORD()
                        if self.api.GetNumberOfConsoleInputEvents(handles[0], C.byref(pending)):
                            if pending.value == 0:
                                break
                        elif self.poll() is not None and command_index == len(commands) - 1:
                            break
                        else:
                            raise C.WinError(C.get_last_error(), 'Observe owned input consumption')
                        if time.monotonic() > deadline:
                            raise TimeoutError('owned console reader did not consume command')
                        time.sleep(0.01)
                finally:
                    self.detach(handles)

    def close(self):
        if self.closed:
            return
        if self.poll() is None:
            self.text()
            try:
                self.send(['quit'])
                self.wait(5)
            except (OSError, TimeoutError):
                self.forced = True
                self.check(self.api.TerminateProcess(self.info.hProcess, 1), 'Terminate owned failed GUI console')
                self.wait(5)
        self.exit_code = self.poll()
        self.api.CloseHandle(self.info.hProcess)
        self.info.hProcess = None
        self.closed = True


def await_text(process, condition, name, timeout=45, commands=None):
    deadline = time.monotonic() + timeout
    next_command = 0
    while True:
        text = process.text()
        if condition(text):
            return text
        code = process.poll()
        if code is not None:
            raise RuntimeError(f'{name}: process exited {code}; see raw/text console log')
        if time.monotonic() > deadline:
            raise TimeoutError(name)
        if commands and time.monotonic() >= next_command:
            process.send(commands)
            next_command = time.monotonic() + 1
        time.sleep(0.1)


def collect_modules(process, role, args, expected):
    raw = args.evidence / (role + '-modules.txt')
    subprocess.run([str(args.module_collector), str(process.pid), str(raw)], check=True, capture_output=True,
                   creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
    matched = []
    for text in raw.read_text(encoding='utf-8-sig').splitlines():
        path = Path(text)
        try:
            relative = path.relative_to(args.sandbox).as_posix()
        except ValueError:
            continue
        key = relative.lower()
        if key not in expected:
            raise ValueError('unexpected owned module: ' + relative)
        value = digest(path)
        if value != expected[key]['sha256']:
            raise ValueError('loaded module differs: ' + relative)
        destination = args.evidence / 'images' / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, destination)
        matched.append({'relative': relative, 'sha256': value, 'bytes': path.stat().st_size})
    required = {'hlds.exe', 'swds.dll', 'filesystem_stdio.dll', 'cstrike/dlls/mp.dll'} if role == 'dedicated' else {
        'hltv.exe', 'proxy.dll', 'core.dll', 'filesystem_stdio.dll'}
    if not required.issubset({item['relative'].lower() for item in matched}):
        raise ValueError('missing required loaded modules: ' + role)
    save(args.evidence / (role + '-modules.json'), matched)
    return matched


def run(args):
    expected = {item['relative'].lower(): item for item in json.loads(args.identities.read_text(encoding='utf-8-sig'))}
    result = {'success': False, 'configuration': args.configuration, 'python_pid': os.getpid(),
              'scope': ('owned loopback LAN/insecure dedicated lifecycle' if args.dedicated_only else
                        'owned loopback LAN/insecure dedicated + HLTV lifecycle') + '; no authentication parity',
              'transport': 'retained native console' if args.retained_console else 'headless ConPTY',
              'hltv_master_module': None if args.dedicated_only else
                                    'unloaded using retained command before connect'}
    processes = []
    try:
        server_command = [args.sandbox / 'hlds.exe', '-console', '1', '-insecure', '-nomaster', '-dev', '-condebug',
                          '-game', 'cstrike', '+ip', '127.0.0.1', '-port', str(args.server_port), '+sv_lan', '1',
                          '+maxplayers', '4', '+map', 'de_dust2']
        result['dedicated_command'] = [str(item) for item in server_command]
        if args.retained_console:
            server = RetainedConsoleProcess(server_command, args.sandbox, args.evidence / 'dedicated-console.snapshot',
                                            [args.sandbox / 'qconsole.log', args.sandbox / 'cstrike/qconsole.log'])
        else:
            server = ConsoleProcess(server_command, args.sandbox, args.evidence / 'dedicated-console.raw')
        processes.append(server)
        result['dedicated_pid'] = server.pid
        result['dedicated_session'] = server.session_id
        save(args.evidence / 'progress.json', result)
        await_text(server, lambda text: re.search(r'map\s*:\s*de_dust2\b', text) is not None,
                   'dedicated initial map/status', commands=['status'])
        result['dedicated_modules'] = collect_modules(server, 'dedicated', args, expected)
        script = (f'Get-NetUDPEndpoint -OwningProcess {server.pid} | '
                  'Select-Object LocalAddress,LocalPort,OwningProcess | ConvertTo-Json -Compress')
        endpoints = subprocess.run(['powershell.exe', '-NoProfile', '-Command', script], check=True,
                                   capture_output=True, text=True, creationflags=subprocess.CREATE_NO_WINDOW)
        result['dedicated_udp_endpoints'] = json.loads(endpoints.stdout)
        endpoint_list = result['dedicated_udp_endpoints']
        if isinstance(endpoint_list, dict):
            endpoint_list = [endpoint_list]
        game_endpoints = [item for item in endpoint_list if item['LocalPort'] == args.server_port]
        if not game_endpoints or any(item['LocalAddress'] != '127.0.0.1' for item in game_endpoints):
            raise ValueError('owned dedicated game endpoint is not loopback-only')
        if args.dedicated_only:
            server.send(['sv_lan', 'mp_consistency', 'version', 'status'])
            await_text(server, lambda text: re.search(r'"sv_lan" is "1(?:\.0*)?"', text) is not None
                       and re.search(r'"mp_consistency" is "1(?:\.0*)?"', text) is not None,
                       'actual LAN/consistency cvar queries', commands=['sv_lan', 'mp_consistency'])
            result['observed_cvars'] = {'sv_lan': 1, 'mp_consistency': 1}
            server.send(['changelevel de_dust'])
            await_text(server, lambda text: re.search(r'map\s*:\s*de_dust\b', text) is not None,
                       'dedicated changelevel/status', commands=['status'])
            result['maps'] = ['de_dust2', 'de_dust']
            server.send(['quit'])
            result['dedicated_exit_code'] = server.wait(20)
        else:
            hltv_command = [args.sandbox / 'hltv.exe', '-novgui', '-nodns', '-ip', '127.0.0.1', '-port', str(args.hltv_port),
                            '+unloadmodule', 'master000', '+publicgame', '0', '+autoretry', '1', '+delay', '0',
                            '+logfile', 'cse_lifecycle_hltv.txt', '+connect', f'127.0.0.1:{args.server_port}']
            result['hltv_command'] = [str(item) for item in hltv_command]
            if args.retained_console:
                hltv = RetainedConsoleProcess(hltv_command, args.sandbox, args.evidence / 'hltv-console.snapshot',
                                             [args.sandbox / folder / 'cse_lifecycle_hltv.txt'
                                              for folder in ('', 'valve', 'cstrike')])
            else:
                hltv = ConsoleProcess(hltv_command, args.sandbox, args.evidence / 'hltv-console.raw')
            processes.append(hltv)
            result['hltv_pid'] = hltv.pid
            result['hltv_session'] = hltv.session_id
            save(args.evidence / 'progress.json', result)
            await_text(hltv, lambda text: 'Connected to Game Server 127.0.0.1:' in text and 'Map "de_dust2.bsp"' in text,
                       'HLTV actual first world/status', commands=['status'])
            result['hltv_modules'] = collect_modules(hltv, 'hltv', args, expected)
            server.send(['changelevel de_dust'])
            await_text(server, lambda text: re.search(r'map\s*:\s*de_dust\b', text) is not None,
                       'dedicated changelevel/status', commands=['status'])
            await_text(hltv, lambda text: 'Map "de_dust.bsp"' in text,
                       'HLTV actual changed world/status', commands=['status'])
            result['maps'] = ['de_dust2', 'de_dust']
            hltv.send(['disconnect', 'quit'])
            result['hltv_exit_code'] = hltv.wait(20)
            server.send(['quit'])
            result['dedicated_exit_code'] = server.wait(20)
        if result.get('hltv_exit_code', 0) or result['dedicated_exit_code']:
            raise RuntimeError('nonzero graceful server/HLTV exit')
        result['success'] = True
    except Exception as error:
        result['failure'] = f'{type(error).__name__}: {error}'
    finally:
        for process in reversed(processes):
            try:
                process.close()
                result[str(process.pid) + '_forced_cleanup'] = process.forced
                result[str(process.pid) + '_cleanup_exit_code'] = process.exit_code
            except Exception as error:
                result['success'] = False
                result['cleanup_failure'] = str(error)
            (args.evidence / (('dedicated' if process == processes[0] else 'hltv') + '-console.txt')).write_text(process.text())
            if isinstance(process, RetainedConsoleProcess):
                result[str(process.pid) + '_console_observation'] = process.console_observation
        save(args.evidence / 'result.json', result)
    return 0 if result['success'] else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sandbox', type=Path, required=True)
    parser.add_argument('--evidence', type=Path, required=True)
    parser.add_argument('--identities', type=Path, required=True)
    parser.add_argument('--module-collector', type=Path, required=True)
    parser.add_argument('--configuration', choices=('Release', 'Debug'), required=True)
    parser.add_argument('--server-port', type=int, default=27441)
    parser.add_argument('--hltv-port', type=int, default=27442)
    parser.add_argument('--dedicated-only', action='store_true', help='Do not launch HLTV')
    parser.add_argument('--retained-console', action='store_true',
                        help='Allow the GUI launchers to create their owned developer console windows')
    return run(parser.parse_args())


if __name__ == '__main__':
    raise SystemExit(main())
