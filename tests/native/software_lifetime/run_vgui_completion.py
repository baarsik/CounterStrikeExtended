"""Actual CVGui shutdown/finish bodies, controlled container and selected-view boundaries."""
import pathlib
import subprocess
import sys
root = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tests/native/server_browser_ui'))
from extract_owners import extract
out = pathlib.Path(sys.argv[1]).resolve()
out.mkdir(parents=True, exist_ok=True)
source = (root / 'src/vgui2/src/vgui.cpp').read_text(encoding='latin1')
baseline = len(sys.argv) > 2 and sys.argv[2] == '--baseline'
if baseline:
    body, _ = extract(source, 'CVGui::Shutdown', 0, 1)
else:
    start = source.index('cse::vgui_completion::Result CVGui::FinishNativeClient(')
    from extract_owners import closing_brace
    body = source[start:closing_brace(source, source.index('{', start))]
(out / 'actual_vgui_completion.inc').write_text(body)
command = ['clang++', '-std=c++17', ('-Os' if '--release' in sys.argv else '-O0'), '-UNDEBUG', '-Wall', '-Wextra', '-Werror',
           '-I' + str(root / 'src/public'), '-I' + str(out),
           str(root / 'tests/native/software_lifetime/vgui_completion.cpp'), '-o', str(out / 'vgui_completion')]
if baseline:
    command.append('-DCSE_COMPLETION_BASELINE=1')
subprocess.run(command, check=True)
subprocess.run([str(out / 'vgui_completion')], check=True)
