#!/usr/bin/env python3
"""Compile real standalone producer/control recipes and require the frozen public physical ABI."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SCHEMA = ROOT / 'tests/native/vgui2_standalone/abi-schema.json'
PROBE = '''
#include "vgui/ISurface.h"
#include "vgui/IVGui.h"
#include "vgui/IPanel.h"
#include "vgui/IInputInternal.h"
#include "vgui/ISystem.h"
#include "vgui/ILocalize.h"
#include "vgui/IScheme.h"
#include "IKeyValues.h"
void SurfaceEvidence(vgui2::ISurface* p) { p->Shutdown(); }
void GuiEvidence(vgui2::IVGui* p) { p->Start(); }
void PanelEvidence(vgui2::IPanel* p) { p->SetVisible(0, false); }
void InputEvidence(vgui2::IInput* p) { p->GetFocus(); }
void InternalEvidence(vgui2::IInputInternal* p) { p->RunFrame(); }
void SystemEvidence(vgui2::ISystem* p) { p->RunFrame(); }
void LocalizeEvidence(vgui2::ILocalize* p) { p->RemoveAll(); }
void SchemeEvidence(vgui2::ISchemeManager* p) { p->GetDefaultScheme(); }
void SchemeViewEvidence(vgui2::IScheme* p) { p->GetFont("", false); }
void KeyValuesEvidence(IKeyValues* p) { p->GetSymbolForString(""); }
void FileSystemEvidence(IFileSystem* p) { p->Mount(); }
void CallbackEvidence(vgui2::IClientPanel* p) { p->Think(); }
static_assert(sizeof(vgui2::VPANEL) == sizeof(void*));
static_assert(sizeof(vgui2::HPanel) == 4 && sizeof(vgui2::HTexture) == 4 && sizeof(vgui2::HFont) == 4);
static_assert(sizeof(vgui2::StringIndex_t) == 4 && sizeof(int) == 4 && sizeof(wchar_t) == 4);
static_assert(sizeof(Color) == 4 && alignof(Color) == 1);
static_assert(sizeof(KeyValues) == 48 && alignof(KeyValues) == 8);
'''
NAMES = ['vgui2::ISurface', 'vgui2::IVGui', 'vgui2::IPanel', 'vgui2::IInput', 'vgui2::IInputInternal',
         'vgui2::ISystem', 'vgui2::ILocalize', 'vgui2::ISchemeManager', 'vgui2::IScheme',
         'IKeyValues', 'IFileSystem', 'vgui2::IClientPanel']
CONTEXTS = [('vgui2', '/vgui2/src/SurfaceSDL.cpp'), ('hlds', '/vgui2/controls/controls.cpp')]


def digest(data):
    return hashlib.sha256(data).hexdigest()


def facts(text):
    found = {}
    pattern = r"VTable indices for '([^']+)' \(\d+ entries\)\.\n(.*?)(?=\n\n)"
    for name, body in re.findall(pattern, text, re.S):
        if name in NAMES:
            rows = []
            for line in body.splitlines():
                match = re.match(r'\s*(\d+) \| (.*)', line)
                if match:
                    signature = match[2].replace('class ', '').replace('enum ', '')
                    signature = re.sub(r'\s+', ' ', signature)
                    rows.append({'slot': int(match[1]), 'signature': signature})
            # Public filesystem is a prefix of the internal provider declaration.
            if name == 'IFileSystem':
                rows = rows[:56]
            found[name] = rows
    if set(found) != set(NAMES):
        raise RuntimeError(f'Missing actual compiler interface dumps: {set(NAMES) - set(found)}')
    return found


def records(text):
    result = {}
    for name in NAMES + ['Color', 'KeyValues']:
        pattern = r'\s*0 \| class ' + re.escape(name) + r'\n(.*?nvsize=.*?\])'
        match = re.search(pattern, text, re.S)
        if not match:
            raise RuntimeError('Missing physical record: ' + name)
        value = match[1]
        value = re.sub(r'\(anonymous at [^)]*\)', '(anonymous union)', value)
        # Public KeyValues additionally names the same pointer-sized union storage
        # wchar_t*. The consumed union byte offsets and all other aliases stay checked.
        value = '\n'.join(line.strip() for line in value.splitlines() if 'm_wsValue' not in line)
        result[name] = value
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--database', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--dedicated-consumers', action='store_true')
    options = parser.parse_args()
    options.output = options.output.resolve()
    options.output.mkdir(parents=True, exist_ok=True)
    stamp = options.output / 'accepted.json'
    stamp.unlink(missing_ok=True)
    schema = json.loads(SCHEMA.read_text())
    signature = digest(SCHEMA.read_bytes())[:16]
    header = (ROOT / "src/public/native_standalone_abi.h").read_text()
    if "0x" + signature + "ull" not in header:
        raise RuntimeError("Runtime descriptor is not bound to the frozen physical schema")
    database = json.loads(options.database.read_text())
    receipts = []
    contexts = [('hlds', '/dedicated/native/DedicatedModuleSet.cpp'),
                ('hlds', '/dedicated/native/NativeGui.cpp')] if options.dedicated_consumers else CONTEXTS
    for target, suffix in contexts:
        selected = [row for row in database if row['file'].endswith(suffix) and f'targets/{target}/' in row['command']]
        if len(selected) != 1:
            raise RuntimeError(f'Expected actual owning recipe: {target} {suffix}')
        row = selected[0]
        label = target + '-' + Path(row['file']).stem
        source = options.output / (label + '.cpp')
        extra = '#include "' + str(ROOT / 'src/cstrike/public/vgui/KeyValues.h') + '"\n' \
            if options.dedicated_consumers else ''
        probe = ('#include "native_standalone_interfaces.h"\n' + PROBE[PROBE.index('void SurfaceEvidence'):]) \
            if options.dedicated_consumers else PROBE
        source.write_text('#include "' + row['file'] + '"\n' + extra + probe)
        args = shlex.split(row['command'])
        clean = []
        skip = False
        for arg in args:
            if skip:
                skip = False
            elif arg in ['-o', '-MF', '-MT', '-MQ']:
                skip = True
            elif arg.startswith('@'):
                raise RuntimeError('Unqualified response-file owning recipe')
            elif arg not in ['-MD', '-MMD', '-c', row['file']]:
                clean.append(arg)
        command = clean + ['-H', '-Xclang', '-fdump-vtable-layouts', '-Xclang', '-fdump-record-layouts',
                           '-S', '-emit-llvm', str(source), '-o', str(options.output / (label + '.ll'))]
        result = subprocess.run(command, cwd=row['directory'], capture_output=True, timeout=180)
        log = options.output / (label + '.log')
        log.write_bytes(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(f'Actual owning compile failed: {log}')
        current = facts(log.read_text(errors='replace'))
        current["_records"] = records(log.read_text(errors="replace"))
        if current != schema:
            (options.output / (label + '-actual.json')).write_text(json.dumps(current, indent=2))
            raise RuntimeError(f'Public physical method signature/order mismatch: {label}')
        preprocessed = subprocess.run(clean + ['-E', '-P', str(source)], cwd=row['directory'],
                                      capture_output=True, timeout=180, check=True)
        compiler = subprocess.run([clean[0], '--version'], capture_output=True, check=True).stdout
        receipts.append(dict(owner=row, owner_sha256=digest(Path(row['file']).read_bytes()), command=command,
                             compiler_sha256=digest(Path(clean[0]).read_bytes()), compiler=compiler.decode(),
                             preprocessed_sha256=digest(preprocessed.stdout), log_sha256=digest(log.read_bytes()),
                             physical_slots=current,
                             include_provenance=[line for line in result.stderr.decode(errors='replace').splitlines()
                                                 if re.match(r'^\.+ /', line)]))
    stamp.write_text(json.dumps({'schema_sha256': digest(SCHEMA.read_bytes()), 'receipts': receipts,
        'scope': 'nine complete factory views plus internal input, scheme view and client panel callback'}, indent=2))
    print('Accepted standalone VGUI physical ABI:', len(receipts), 'actual owning contexts')


if __name__ == '__main__':
    main()
