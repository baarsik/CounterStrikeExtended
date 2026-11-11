#!/usr/bin/env python3
"""Compile the actual dedicated class/state and methods with controlled external owners."""
import pathlib
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tests/native/dedicated_host'))
from actual_arguments import definitions

out = pathlib.Path(sys.argv[1]).resolve()
out.mkdir(parents=True, exist_ok=True)
source = (root / 'GoldSrc-SDK/engine/sys_dll2.cpp').read_text()
interfaces = '#include "idedicatedexports.h"\n#include "dedicated_api.h"\n'

owner = source[source.index('class CDedicatedServerAPI :'):source.index('\n#ifdef _MSC_VER', source.index('class CDedicatedServerAPI :'))]
start = source.index('bool CDedicatedServerAPI::Init(char *')
methods = source[start:source.index('\n#else\nbool CDedicatedServerAPI::Init(', start)]
fixture = out / 'dedicated_owner_actual.cpp'
fixture.write_text('#include "linked_owner.h"\n#include "dedicated_owner_services.h"\n#include "engine/native_software_lifetime.h"\n' + definitions(root) + interfaces + owner +
                   '\nstatic IDedicatedExports *dedicated;\n' + methods +
                   '\n#include "dedicated_owner_fixture.cpp"\n')
optimization = sys.argv[2] if len(sys.argv) > 2 else '-O0'
assert optimization in ('-O0', '-O2', '-Os')
command = ['clang++', optimization, '-ffp-model=precise', '-ffp-contract=off', '-Wno-overriding-option', '-std=c++17', '-UNDEBUG', '-Wall', '-Wextra', '-Werror',
           '-I' + str(root / 'GoldSrc-SDK/public'), '-I' + str(root / 'GoldSrc-SDK'),
           '-I' + str(root / 'tests/native/software_video'), str(fixture), str(root / 'GoldSrc-SDK/engine/native_software_lifetime.cpp'),
           '-Wl,-export_dynamic', '-o', str(out / 'dedicated_owner_test')]
sys.path.insert(0, str(root / 'tests/native/socket_lifetime'))
from linked_owner import arguments
command += arguments(root, out, optimization)
subprocess.run(command, check=True)
subprocess.run([str(out / 'dedicated_owner_test')], check=True)
