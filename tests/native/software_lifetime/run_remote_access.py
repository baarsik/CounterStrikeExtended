"""Actual admin complete-call bodies and queue, with controlled command/resource leaves."""
import hashlib
import json
import pathlib
import subprocess
import sys
root = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tests/native/server_browser_ui'))
from extract_owners import extract, closing_brace
out = pathlib.Path(sys.argv[1]).resolve()
out.mkdir(parents=True, exist_ok=True)
owner = root / 'GoldSrc-SDK/engine/sv_remoteaccess.cpp'
source = owner.read_text()
start = source.index('class CServerRemoteAccess :')
end = source.index('\n};', start) + 3
body = source[start:end].replace('private:', 'public:')
start = source.index('CServerRemoteAccess::CServerRemoteAccess()')
body += '\n' + source[start:closing_brace(source, source.index('{', start))]
start = source.index('CServerRemoteAccess::~CServerRemoteAccess()')
body += '\n' + source[start:closing_brace(source, source.index('{', start))]
for name in ['CServerRemoteAccess::WriteDataRequest', 'CServerRemoteAccess::ReadDataResponse',
             'CServerRemoteAccess::SendMessageToAdminUI']:
    body += '\n' + extract(source, name, 0, 1)[0]
if 'CServerRemoteAccess::RetireResponses' in source:
    start = source.index('bool CServerRemoteAccess::RetireResponses() noexcept')
    body += '\n' + source[start:closing_brace(source, source.index('{', start))]
(out / 'remote_owner.inc').write_text(body)
command = ['clang++', '-std=c++17', ('-Os' if '--release' in sys.argv else '-O0'), '-UNDEBUG', '-DSW_RENDERER', '-D_vsnprintf=vsnprintf',
           '-I' + str(out), '-I' + str(root / 'GoldSrc-SDK'), '-I' + str(root / 'GoldSrc-SDK/engine'),
           '-I' + str(root / 'GoldSrc-SDK/public'),
           str(root / 'tests/native/software_lifetime/remote_access.cpp'),
           str(root / 'GoldSrc-SDK/engine/native_software_lifetime.cpp'),
           str(root / 'GoldSrc-SDK/engine/utlbuffer.cpp'), '-o', str(out / 'remote_access')]
if 'CServerRemoteAccess::RetireResponses' not in source:
    command.append('-DCSE_REMOTE_BASELINE')
run = subprocess.run(command, capture_output=True)
(out / 'compile.log').write_bytes(run.stdout + run.stderr)
receipt = {'command': command, 'source_sha256': hashlib.sha256(owner.read_bytes()).hexdigest(),
           'extracted_sha256': hashlib.sha256(body.encode()).hexdigest(), 'compile_exit': run.returncode}
if run.returncode == 0:
    run = subprocess.run([str(out / 'remote_access')], capture_output=True)
    (out / 'run.log').write_bytes(run.stdout + run.stderr)
    receipt['run_exit'] = run.returncode
print((run.stdout + run.stderr).decode())
(out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
sys.exit(run.returncode)
