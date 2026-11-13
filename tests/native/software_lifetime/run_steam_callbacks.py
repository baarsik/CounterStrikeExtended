"""Extract the real SDK registration, pump, and retirement owners; only SDK calls are controlled."""
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
owner = root / 'src/common/steam_service.cpp'
source = owner.read_text()
names = ['SteamAPI_Init', 'SteamAPI_Shutdown', 'SteamInternal_GameServer_Init', 'SteamGameServer_Shutdown', 'SteamAPI_RunCallbacks', 'SteamGameServer_RunCallbacks', 'SteamAPI_RegisterCallback',
         'SteamAPI_UnregisterCallback', 'SteamAPI_RegisterCallResult', 'SteamAPI_UnregisterCallResult']
terminal = 'CseSteamRetireModuleCallbacks' in source
names += ['CseSteamRetireModuleCallbacks' if terminal else 'CseSteamRetireServerTransport']
start = source.index('struct NativeProvider {')
end = source.index('} // namespace', start)
body = source[start:end] + '\n' + '\n'.join(extract(source, name, 0, 1)[0] for name in names[:-1])
start = source.index('bool ' + names[-1] + '()')
body += '\n' + source[start:closing_brace(source, source.index('{', start))]
(out / 'steam_callbacks.inc').write_text(body)
command = ['clang++', '-std=c++17', '-DSW_RENDERER', ('-Os' if '--release' in sys.argv else '-O0'), '-UNDEBUG', '-Wall', '-Wextra', '-Werror',
           '-I' + str(out), str(root / 'tests/native/software_lifetime/steam_callbacks.cpp'),
           '-o', str(out / 'steam_callbacks')]
if not terminal:
    command.append('-DCSE_STEAM_BASELINE')
run = subprocess.run(command, capture_output=True)
(out / 'compile.log').write_bytes(run.stdout + run.stderr)
receipt = {'source_sha256': hashlib.sha256(owner.read_bytes()).hexdigest(),
           'extracted_sha256': hashlib.sha256(body.encode()).hexdigest(), 'command': command,
           'compile_exit': run.returncode}
if run.returncode == 0:
    run = subprocess.run([str(out / 'steam_callbacks')], capture_output=True)
    receipt['run_exit'] = run.returncode
    (out / 'run.log').write_bytes(run.stdout + run.stderr)
print((run.stdout + run.stderr).decode())
(out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
sys.exit(run.returncode)
