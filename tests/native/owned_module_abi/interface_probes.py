#!/usr/bin/env python3
"""Record real header resolution and selected typed vtable facts without module execution."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

from compile_probes import owner, prefix, walk

ROOT = Path(__file__).resolve().parents[3]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--database', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    options = parser.parse_args()
    options.output.mkdir(parents=True, exist_ok=True)
    database = json.loads(options.database.read_text())
    rows = []
    contexts = [
        ('vgui2', '/vgui2/src/vgui.cpp'),
        ('hw_engine', '/engine/vgui2/BaseUI_Interface.cpp'),
        ('gameui', '/GameUI/GameUI_Interface.cpp'),
        ('cs_client', '/cstrike/vgui2/controls/controls.cpp')]
    for target, suffix in contexts:
        entry = owner(database, suffix, target)
        command = prefix(entry) + ['-E', '-H', entry['file'], '-o', '/dev/null']
        run = subprocess.run(command, cwd=entry['directory'], capture_output=True, timeout=120)
        (options.output / (target + '-headers.log')).write_bytes(run.stdout + run.stderr)
        rows.append({'case': target + '-actual-TU-preprocess', 'args': command,
                     'exit': run.returncode, 'qualification': 'preprocessing only; not a production TU compile'})

    probes = [
        ('ivgui-provider', 'vgui2', '/vgui2/src/vgui.cpp',
         'vgui2/src/public/vgui/IVGui.h', 'vgui2::IVGui', 'RunFrame()'),
        ('ivgui-engine', 'hw_engine', '/engine/vgui2/BaseUI_Interface.cpp',
         'engine/vgui/IVGui.h', 'vgui2::IVGui', 'RunFrame()'),
        ('surface-provider', 'vgui2', '/vgui2/src/Surface.cpp',
         'vgui2/src/public/vgui/ISurface.h', 'vgui2::ISurface', 'Shutdown()'),
        ('surface-engine', 'hw_engine', '/engine/vgui2/BaseUI_Interface.cpp',
         'engine/vgui/ISurface.h', 'vgui2::ISurface', 'Shutdown()')]
    signatures = []
    for label, target, suffix, header, interface, invocation in probes:
        entry = owner(database, suffix, target)
        source = options.output / (label + '.cpp')
        source.write_text('#include "' + str(ROOT / 'src' / header)
                          + '"\nvoid Probe(' + interface + '* value) {\n    value->' + invocation + ';\n}\n')
        command = prefix(entry) + ['-Xclang', '-fdump-vtable-layouts', '-S', '-emit-llvm', str(source),
                                   '-o', str(options.output / (label + '.ll'))]
        run = subprocess.run(command, cwd=entry['directory'], capture_output=True, timeout=120)
        (options.output / (label + '-vtable.log')).write_bytes(run.stdout + run.stderr)
        rows.append({'case': label + '-vtable', 'args': command, 'exit': run.returncode,
                     'qualification': 'actual header under owning TU flags; compile only'})
        command = prefix(entry) + ['-fsyntax-only', '-Xclang', '-ast-dump=json', str(source)]
        run = subprocess.run(command, cwd=entry['directory'], capture_output=True, timeout=120)
        (options.output / (label + '.ast.json')).write_bytes(run.stdout)
        (options.output / (label + '-ast.log')).write_bytes(run.stderr)
        rows.append({'case': label + '-ast', 'args': command, 'exit': run.returncode})
        if run.returncode == 0:
            name = interface.split('::')[-1]
            records = [node for node in walk(json.loads(run.stdout)) if node.get('kind') == 'CXXRecordDecl'
                       and node.get('name') == name and node.get('completeDefinition')]
            assert len(records) == 1
            methods = [node for node in records[0].get('inner', [])
                       if node.get('kind') in ['CXXMethodDecl', 'CXXDestructorDecl']
                       and not node.get('isImplicit')]
            signatures.append({'case': label, 'header': header, 'bases': records[0].get('bases'),
                               'method_declarations_not_physical_slots': [
                                   {'name': method['name'], 'type': method['type']['qualType'],
                                    'virtual': method.get('virtual', False)} for method in methods]})
    compiler = Path(prefix(owner(database, contexts[0][1], contexts[0][0]))[0]).resolve()
    (options.output / 'receipt.json').write_text(json.dumps({
        'compiler': str(compiler), 'compiler_sha256': hashlib.sha256(compiler.read_bytes()).hexdigest(),
        'database_sha256': hashlib.sha256(options.database.read_bytes()).hexdigest(), 'rows': rows,
        'execution': 'compiler only; no generated binary linked, loaded or executed'}, indent=2))
    (options.output / 'typed-interface-facts.json').write_text(json.dumps(signatures, indent=2))
    for row in rows:
        print(row['case'], row['exit'])
    if any(row['exit'] != 0 for row in rows):
        raise SystemExit(1)


if __name__ == '__main__':
    main()
