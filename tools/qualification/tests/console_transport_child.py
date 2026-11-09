"""Private native-console control program; never loads game/runtime images."""
import argparse
import ctypes as C
from ctypes import wintypes as W
import json
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from server_lifecycle_probe import STARTUPINFO


class KEYCHAR(C.Union):
    _fields_ = [('UnicodeChar', W.WCHAR), ('AsciiChar', C.c_char)]


class KEYEVENT(C.Structure):
    _fields_ = [('down', W.BOOL), ('repeat', W.WORD), ('virtual_key', W.WORD),
                ('scan', W.WORD), ('character', KEYCHAR), ('control', W.DWORD)]


class EVENTDATA(C.Union):
    _fields_ = [('key', KEYEVENT), ('other', C.c_byte * 16)]


class INPUTRECORD(C.Structure):
    _fields_ = [('kind', W.WORD), ('event', EVENTDATA)]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--receipt', type=Path, required=True)
    parser.add_argument('--mode', choices=('probe', 'events', 'large', 'sleep'), default='probe')
    args = parser.parse_args()
    api = C.WinDLL('kernel32', use_last_error=True)
    api.GetStdHandle.argtypes = [W.DWORD]
    api.GetStdHandle.restype = W.HANDLE
    api.GetFileType.argtypes = [W.HANDLE]
    api.GetFileType.restype = W.DWORD
    api.GetConsoleMode.argtypes = [W.HANDLE, C.POINTER(W.DWORD)]
    api.GetConsoleMode.restype = W.BOOL
    api.GetStartupInfoW.argtypes = [C.POINTER(STARTUPINFO)]
    api.GetConsoleWindow.restype = W.HWND
    api.WriteConsoleW.argtypes = [W.HANDLE, W.LPCWSTR, W.DWORD, C.POINTER(W.DWORD), C.c_void_p]
    api.WriteConsoleW.restype = W.BOOL

    def output(text):
        count = W.DWORD()
        if not api.WriteConsoleW(api.GetStdHandle((-11) & 0xffffffff), text, len(text), C.byref(count), None):
            raise C.WinError(C.get_last_error())
        if count.value != len(text):
            raise RuntimeError('partial console write')

    startup = STARTUPINFO()
    api.GetStartupInfoW(C.byref(startup))
    handles = {}
    for name, index in [('stdin', -10), ('stdout', -11), ('stderr', -12)]:
        handle = api.GetStdHandle(index & 0xffffffff)
        mode = W.DWORD()
        handles[name] = {'handle': handle, 'file_type': api.GetFileType(handle),
                         'console_mode_ok': bool(api.GetConsoleMode(handle, C.byref(mode))), 'mode': mode.value}
    result = {'startup_flags': startup.dwFlags, 'startup_cb': startup.cb,
              'handles': handles, 'console_window': api.GetConsoleWindow()}
    args.receipt.write_text(json.dumps(result, indent=2) + '\n')
    if args.mode == 'probe':
        print('CSE_NATIVE_CONSOLE_PROBE', flush=True)
    elif args.mode == 'sleep':
        time.sleep(30)
    elif args.mode == 'large':
        for index in range(512):
            output(f'CSE_LINE_{index:04d}:' + 'x' * 64 + '\r\n')
        output('CSE_LARGE_OUTPUT_END\r\n')
    elif args.mode == 'events':
        # Exact Windows layout used by the retained ReadConsoleInputA readers.
        assert C.sizeof(KEYEVENT) == 16 and C.sizeof(INPUTRECORD) == 20
        api.ReadConsoleInputA.argtypes = [W.HANDLE, C.POINTER(INPUTRECORD), W.DWORD, C.POINTER(W.DWORD)]
        api.ReadConsoleInputA.restype = W.BOOL
        output('CSE_INPUT_RECORDS_READY\r\n')
        line = bytearray()
        while True:
            records = (INPUTRECORD * 32)()
            count = W.DWORD()
            if not api.ReadConsoleInputA(api.GetStdHandle((-10) & 0xffffffff), records, len(records),
                                         C.byref(count)):
                raise C.WinError(C.get_last_error())
            for record in records[:count.value]:
                if record.kind != 1 or not record.event.key.down:
                    continue
                character = record.event.key.character.AsciiChar
                if character == b'\r':
                    text = line.decode('ascii')
                    output('CSE_INPUT_RECORDS_RECEIVED:' + text + '\r\n')
                    result['received_line'] = text
                    result['return_virtual_key'] = record.event.key.virtual_key
                    args.receipt.write_text(json.dumps(result, indent=2) + '\n')
                    return 0 if text in ('CSE_DELAYED_INPUT', 'quit') else 2
                if character and b' ' <= character <= b'~':
                    line.extend(character)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
