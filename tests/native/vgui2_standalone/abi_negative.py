"""Exercise the production physical schema checker with mutated real public headers."""
import importlib.util
import json
from pathlib import Path
import shlex
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
spec = importlib.util.spec_from_file_location('evidence', root / 'tools/standalone_vgui_abi_evidence.py')
evidence = importlib.util.module_from_spec(spec)
spec.loader.exec_module(evidence)
database = json.loads(Path(sys.argv[1]).read_text())
output = Path(sys.argv[2]).resolve()
output.mkdir(parents=True, exist_ok=True)
expected = json.loads(evidence.SCHEMA.read_text())
public = root / 'GoldSrc-SDK/vgui2/src/public'
surface = (public / 'vgui/ISurface.h').read_text(encoding='latin1')
first = '\tvirtual void DrawSetColor( int r, int g, int b, int a ) = 0;'
second = '\tvirtual void DrawSetColor( Color color ) = 0;'
assert first + '\n' + second in surface
reordered = output / 'reordered_surface.h'
reordered.write_text(surface.replace(first + '\n' + second, second + '\n' + first), encoding='latin1')
color = (public / 'Color.h').read_text(encoding='latin1')
assert '_color[4]' in color
record = output / 'changed_color.h'
record.write_text(color.replace('_color[4]', '_color[5]'), encoding='latin1')
for target, suffix in evidence.CONTEXTS:
    rows = [row for row in database if row['file'].endswith(suffix) and f'targets/{target}/' in row['command']]
    assert len(rows) == 1
    row = rows[0]
    args = shlex.split(row['command'])
    clean = []
    skip = False
    for arg in args:
        if skip:
            skip = False
        elif arg in ['-o', '-MF', '-MT', '-MQ']:
            skip = True
        elif arg not in ['-MD', '-MMD', '-c', row['file']]:
            clean.append(arg)
    source = output / (target + '.cpp')
    source.write_text('#include "' + row['file'] + '"\n' + evidence.PROBE)
    for kind, header in [('methods', reordered), ('record', record)]:
        command = clean + ['-include', str(header), '-Xclang', '-fdump-vtable-layouts',
                           '-Xclang', '-fdump-record-layouts', '-S', '-emit-llvm', str(source),
                           '-o', str(output / (target + '-' + kind + '.ll'))]
        result = subprocess.run(command, cwd=row['directory'], capture_output=True, timeout=180)
        log = (result.stdout + result.stderr).decode(errors='replace')
        (output / (target + '-' + kind + '.log')).write_text(log)
        (output / (target + '-' + kind + '-command.json')).write_text(json.dumps(command, indent=2))
        if kind == 'methods':
            assert result.returncode == 0, log[-2000:]
            actual = evidence.facts(log)
            assert actual['vgui2::ISurface'] != expected['vgui2::ISurface'], 'reordered slots escaped schema gate'
        else:
            assert result.returncode != 0 and 'static assertion failed' in log and 'sizeof(Color)' in log
        print('REJECTED actual owning', target, kind, 'mutation before any runtime factory')
