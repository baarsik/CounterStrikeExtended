"""Compile the current complete BaseUI retirement owner with controlled process/provider leaves."""
import hashlib
import json
import pathlib
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tests/native/server_browser_ui'))
from extract_owners import extract

out = pathlib.Path(sys.argv[1]).resolve()
out.mkdir(parents=True, exist_ok=True)
owner = root / 'GoldSrc-SDK/engine/vgui2/BaseUI_Interface.cpp'
body, line = extract(owner.read_text(), 'CBaseUI::TryShutdown', 0, 1)
(out / 'actual_completion.inc').write_text(body)
command = ['clang++', '-std=c++17', '-DSW_RENDERER', ('-Os' if '--release' in sys.argv else '-O0'), '-UNDEBUG', '-Wall', '-Wextra', '-Werror',
           '-I' + str(out), '-I' + str(root / 'GoldSrc-SDK/public'),
           str(root / 'tests/native/software_lifetime/completion_owner.cpp'), '-o', str(out / 'completion_owner')]
result = subprocess.run(command, capture_output=True)
(out / 'compile.log').write_bytes(result.stdout + result.stderr)
receipt = {'source': str(owner), 'line': line, 'source_sha256': hashlib.sha256(owner.read_bytes()).hexdigest(),
           'body_sha256': hashlib.sha256(body.encode()).hexdigest(), 'command': command, 'compile_exit': result.returncode}
if result.returncode == 0:
    run = subprocess.run([str(out / 'completion_owner')], capture_output=True)
    (out / 'run.log').write_bytes(run.stdout + run.stderr)
    receipt['run_exit'] = run.returncode
    print((run.stdout + run.stderr).decode())
(out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
sys.exit(receipt.get('run_exit', result.returncode))
