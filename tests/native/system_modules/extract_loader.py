"""Extract complete production engine loader methods; services live in fixture.hpp."""
from pathlib import Path
import sys, hashlib
source = Path(sys.argv[1]).read_text()
methods = ['bool SystemWrapper::AddModule(', 'bool SystemWrapper::RemoveModule(',
           'ISystemModule* SystemWrapper::FindModule(', 'ISystemModule* SystemWrapper::GetModule(',
           'void SystemWrapper::RemoveNativeCommands(', 'bool SystemWrapper::RegisterCommand(',
           'bool SystemWrapper::DispatchCommand(', 'bool SystemWrapper::Init(', 'void SystemWrapper::ShutDown()', 'void SystemWrapper::RunFrame(']
out = ['#include "fixture.hpp"\n']
for signature in methods:
    owner_source = source
    actual_signature = signature
    if len(sys.argv) > 3 and signature not in ['bool SystemWrapper::Init(', 'void SystemWrapper::ShutDown()',
                                               'void SystemWrapper::RunFrame(', 'bool SystemWrapper::DispatchCommand(']:
        owner_source = Path(sys.argv[3]).read_text()
        actual_signature = signature.replace('SystemWrapper::', 'System::')
    start = owner_source.index(actual_signature)
    end = owner_source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (owner_source[end] == '{') - (owner_source[end] == '}')
        end += 1
    body = owner_source[start:end].replace('System::', 'SystemWrapper::')
    out.append('// SHA256 source region: ' + hashlib.sha256(body.encode()).hexdigest() + '\n' + body + '\n')
Path(sys.argv[2]).write_text('\n'.join(out))
