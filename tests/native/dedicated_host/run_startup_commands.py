"""Exercise the actual startup reconstruction, buffer, tokenizer and cvar command owners."""
import pathlib
import re
import subprocess
import sys
from actual_arguments import definitions

root = pathlib.Path(__file__).resolve().parents[3]
out = pathlib.Path(sys.argv[1]).resolve()
out.mkdir(parents=True, exist_ok=True)
engine = root / 'GoldSrc-SDK/engine'
def body(file, name):
    source = (engine / file).read_text()
    start = re.search(r'^.*\b' + name + r'\(.*\)\n\{', source, re.M).start()
    end = source.index('\n}\n', start) + 3
    return source[start:end]
parts = [definitions(root), '#include "startup_services.h"\n']
for name in ('Q_IsValidUChar32', 'Q_UTF8ToUChar32', 'V_UTF8ToUChar32', 'Q_UnicodeAdvance'):
    parts.append(body('unicode_strtools.cpp', name))
parts.append(body('common.cpp', 'COM_Parse'))
for name in ('Cbuf_AddTextToBuffer', 'Cbuf_InsertText', 'Cbuf_InsertTextToBuffer',
             'Cbuf_ExecuteCommandsFromBuffer', 'Cmd_TokenizeString', 'Cmd_ExecuteStringWithPrivilegeCheck',
             'Cmd_StuffCmds_f'):
    parts.append(body('cmd.cpp', name))
for name in ('Cvar_IsMultipleTokens', 'Cvar_CommandWithPrivilegeCheck'):
    parts.append(body('cvar.cpp', name))
(out / 'actual_startup.inc').write_text('\n'.join(parts))
command = ['clang++', sys.argv[2], '-std=c++17', '-DSW_RENDERER', '-UNDEBUG', '-Wall', '-Wextra', '-Werror',
           '-Wno-writable-strings', '-Wno-unused-variable', '-ffp-model=precise', '-ffp-contract=off',
           '-Wno-overriding-option', '-I' + str(root / 'GoldSrc-SDK'), '-I' + str(out),
           '-I' + str(root / 'tests/native/dedicated_host'),
           str(root / 'tests/native/dedicated_host/startup_commands.cpp'), '-o', str(out / 'startup_commands')]
subprocess.run(command, check=True)
subprocess.run([str(out / 'startup_commands')], check=True)
