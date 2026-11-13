#!/usr/bin/env python3
"""Exercise title loading through the real CS TextMsg console consumer."""
import importlib.util
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[3]
out = root / 'build/live-localization/cs-console-tests'
out.mkdir(parents=True, exist_ok=True)
spec = importlib.util.spec_from_file_location('extract', root / 'tests/native/network_platform/extract_functions.py')
extract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(extract)
engine = (root / 'src/engine/tmessage.cpp').read_text()
client = (root / 'src/cstrike/cl_dll/text_message.cpp').read_text()
constants = re.findall(
    r'^constexpr int (?:MAX_MESSAGES|NAME_HEAP_SIZE|TEXT_MESSAGE_LINE_BYTES|MSGFILE_NAME|MSGFILE_TEXT) = [^;]+;',
    engine, re.M)
assert len(constants) == 5
engine_functions = [
    'TextMessageGet', 'TrimSpace', 'memfgets', 'IsComment', 'IsStartOfText', 'IsEndOfText', 'IsWhiteSpace',
    'SkipSpace', 'SkipText', 'ParseFloats', 'IsToken', 'ParseDirective', 'TextMessageParse',
    'TextMessageShutdown', 'TextMessageInit',
]
client_functions = [
    'CHudTextMessage::LookupString', 'StripEndNewlineFromString', 'ConvertCRtoNL',
    'CHudTextMessage::MsgFunc_TextMsg',
]
(out / 'engine.inc').write_text('\n'.join(constants) + '\n' + '\n\n'.join(
    extract.extract(engine, name) for name in engine_functions))
(out / 'client.inc').write_text('\n\n'.join(extract.extract(client, name) for name in client_functions))
executable = out / 'console'
subprocess.run([
    'c++', '-std=c++17', '-g', '-O1', '-Wno-writable-strings', '-fsanitize=address,undefined',
    '-fno-omit-frame-pointer',
    '-I', str(root / 'src/engine'), '-I', str(root / 'src/common'), '-I', str(out),
    str(root / 'tests/native/live_localization_cs/console_tests.cpp'), '-o', str(executable),
], check=True)
subprocess.run([str(executable), str(root / 'src/deploy/localization')], check=True)
