#!/usr/bin/env python3
"""Compare actual control sizes and inherited virtual slots against the pinned pre-localization headers."""
import json
from pathlib import Path
import re
import shlex
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
cs_variant = '--cs' in sys.argv
out = root / ('build/live-localization/generic-abi-cs' if cs_variant else 'build/live-localization/generic-abi')
out.mkdir(exist_ok=True)
manifest = json.loads((root / 'build/runtime-macos-arm64-release/compile_commands.json').read_text())
owner = next(item for item in manifest if ('/cs_client.dir/' if cs_variant else '/gameui.dir/') in item['output'] and item['file'].endswith('/Frame.cpp'))
flags = shlex.split(owner['command'])
index = flags.index('-o')
del flags[index:index + 2]
flags.remove('-c')
flags.remove(owner['file'])
flags = [value for value in flags if value != '-Werror']
headers = ['Frame.h', 'ComboBox.h', 'RichText.h', 'TextImage.h', 'Label.h']
baseline = out / 'baseline'
baseline.mkdir(exist_ok=True)
# Quote-includes in Button/EditablePanel resolve beside those headers before -I.
# Copy the complete HEAD control-header closure so a baseline Button cannot
# silently pull today's Label, or define both old and current Label classes.
header_roots = ['src/vgui2/controls']
if cs_variant:
    header_roots.append('src/cstrike/public/vgui_controls')
for header_root in header_roots:
    paths = subprocess.check_output(['git', 'ls-tree', '-r', '--name-only', '538661e0ef6e5d0384bff9f85d0aa451cb93c09e', header_root],
                                    cwd=root, text=True).splitlines()
    for path in paths:
        if path.endswith('.h'):
            (baseline / Path(path).name).write_bytes(
                subprocess.check_output(['git', 'show', '538661e0ef6e5d0384bff9f85d0aa451cb93c09e:' + path], cwd=root))
source = '''#include "Frame.h"
#include "ComboBox.h"
#include "RichText.h"
#include "TextImage.h"
#include "Label.h"
#include <cstdio>
extern "C" void CseSteamModuleInitialize() {}
'''
facts = out / 'facts.cpp'
facts.write_text(source + 'int main() {\n' + '\n'.join(
    'std::printf("' + name[:-2] + ' %zu %zu\\n", sizeof(vgui2::' + name[:-2] + '), alignof(vgui2::' + name[:-2] + '));'
    for name in headers) + '\n}\n')
probes = out / 'probes.cpp'
probes.write_text(source + '''
struct FrameProbe : vgui2::Frame {
    FrameProbe() : Frame(nullptr, "", false, false) {}
    ~FrameProbe() override {}
};
struct ComboProbe : vgui2::ComboBox {
    ComboProbe() : ComboBox(nullptr, "", 3, false) {}
    ~ComboProbe() override {}
};
struct RichProbe : vgui2::RichText {
    RichProbe() : RichText(nullptr, "", false) {}
    ~RichProbe() override {}
};
struct LabelProbe : vgui2::Label {
    LabelProbe() : Label(nullptr, "", "") {}
    ~LabelProbe() override {}
};
void* frame() { return new FrameProbe; }
void* combo() { return new ComboProbe; }
void* rich() { return new RichProbe; }
void* label() { return new LabelProbe; }
''')
results = {}
raw_tables = {}
for mode, include in [('baseline', baseline), ('current', root / ('src/cstrike/public/vgui_controls' if cs_variant else 'src/vgui2/controls'))]:
    executable = out / ('facts-' + mode)
    command = flags[:1] + ['-I' + str(include)] + flags[1:] + [str(facts), '-o', str(executable)]
    result = subprocess.run(command, cwd=owner['directory'], text=True, capture_output=True)
    (out / (mode + '-facts-build.log')).write_text(result.stdout + result.stderr)
    if result.returncode:
        raise SystemExit(result.stderr)
    facts_result = subprocess.check_output([str(executable)], text=True)
    command = flags[:1] + ['-I' + str(include)] + flags[1:] + ['-Xclang', '-fdump-vtable-layouts',
        '-c', str(probes), '-o', str(out / ('probes-' + mode + '.o'))]
    result = subprocess.run(command, cwd=owner['directory'], text=True, capture_output=True)
    (out / (mode + '-vtables.log')).write_text(result.stdout + result.stderr)
    if result.returncode:
        raise SystemExit(result.stderr)
    tables = {}
    for name in ('FrameProbe', 'ComboProbe', 'RichProbe', 'LabelProbe'):
        match = re.search(r"Vtable for '" + name + r"' \((\d+) entries\)\.(.*?)(?=\n\n)", result.stdout, re.S)
        assert match, name
        lines = [line.strip() for line in match.group(2).splitlines() if re.match(r'\s*\d+ \|', line)]
        # An override replaces the inherited implementation at the same ABI slot.
        normalized = [re.sub(r'\b[\w:]+::', '', line) for line in lines]
        tables[name] = {'entries': int(match.group(1)), 'slots': normalized}
        raw_tables.setdefault(mode, {})[name] = lines
    results[mode] = {'facts': facts_result, 'vtables': tables}
(out / 'result.json').write_text(json.dumps(results, indent=2) + '\n')
assert results['baseline'] == results['current'], 'control size/alignment/virtual-slot contract changed'
overrides = {}
for name, old_slots in raw_tables['baseline'].items():
    new_slots = raw_tables['current'][name]
    changed = [(old, new) for old, new in zip(old_slots, new_slots) if old != new]
    # Localization overrides the existing inherited Think slot; it must not
    # replace an unrelated slot or append new virtual entries.
    assert all('::OnThink()' in old and '::OnThink()' in new for old, new in changed), (name, changed)
    overrides[name] = changed
(out / 'overrides.json').write_text(json.dumps(overrides, indent=2) + '\n')
print('PASS: control size/alignment and inherited virtual-slot signatures match pinned pre-localization baseline')
