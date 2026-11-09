"""Compare real Windows-selected preprocessor tokens with includes elided, not a Windows build."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--before', type=Path, required=True)
p.add_argument('--root', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=False)
manifest = json.loads((a.before.parent / 'before-hashes.json').read_text())
rows = []
for name in manifest:
    if not name.endswith(('.cpp', '.h')) or name.endswith('legacy_text_to_integer.h'):
        continue
    before = a.before / name
    current = a.root / name
    # Root owns subsequent KeyValues adaptations. This is our exact alias-only edit.
    after = a.before.parent / 'engine-alias-only-after.cpp' if name.endswith('/engine/KeyValues.cpp') else current
    for client in (False, True):
        tokens = []
        for phase, path in [('before', before), ('after', after)]:
            raw = path.read_bytes()
            stripped = re.sub(rb'^[ \t]*#[ \t]*include[^\r\n]*', b'', raw, flags=re.M)
            text = a.output / (name.replace('/', '_') + ('-client' if client else '-server') + '-' + phase + '.cpp')
            text.write_bytes(stripped)
            command = ['clang++', '-x', 'c++', '-std=c++17', '-D_WIN32=1', '-DWIN32=1',
                       '-D_MSC_VER=1951', '-D_DEBUG=1', '-fsyntax-only', '-Xclang', '-dump-tokens', str(text)]
            if client:
                command.append('-DCLIENT_DLL=1')
            result = subprocess.run(command, capture_output=True, timeout=30)
            assert result.returncode == 0, (name, result.stderr[-1000:])
            (text.with_suffix('.tokens')).write_bytes(result.stderr)
            selected = []
            for line in result.stderr.splitlines():
                if b'Loc=<' in line:
                    selected.append(re.sub(rb'\s*\[(?:StartOfLine|LeadingSpace)\]', b'', line.split(b'Loc=<')[0]).strip())
            tokens.append(selected)
        equal = tokens[0] == tokens[1]
        rows.append({'file': name, 'client_define': client, 'tokens_equal': equal,
                     'selected_token_count': len(tokens[0]),
                     'before_sha256': hashlib.sha256(before.read_bytes()).hexdigest(),
                     'after_sha256': hashlib.sha256(after.read_bytes()).hexdigest()})
        assert equal, name
(a.output / 'result.json').write_text(json.dumps({'scope': 'Clang preprocessor, Windows/MSVC/Debug defines, includes elided; body token invariance, not an actual Windows compiler invocation', 'cases': rows}, indent=2) + '\n')
print(len(rows), 'Windows-selected body/header token controls passed')
