#!/usr/bin/env python3
"""Compile-only actual-header/type/layout probes. Never link, load or execute a module."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent


def owner(database, suffix, target):
    return next(row for row in database if row['file'].endswith(suffix)
                and re.search(r'(^|/)targets/' + target + '/', row['output']))


def prefix(entry):
    args = shlex.split(entry['command'])
    return args[:args.index('-o')]


def walk(node):
    yield node
    for child in node.get('inner', []):
        yield from walk(child)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--database', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    options = parser.parse_args()
    options.output.mkdir(parents=True, exist_ok=True)
    database = json.loads(options.database.read_text())
    rows = []
    core = owner(database, '/HLTV/Core/Network.cpp', 'hltv_core')
    for label, filename in [('actual-runtime9-capacity', 'capacity_probe.cpp'),
                            ('actual-typed-order-negative-controls', 'signature_probe.cpp')]:
        command = prefix(core) + ['-fsyntax-only', str(HERE / filename)]
        run = subprocess.run(command, cwd=core['directory'], capture_output=True, timeout=120)
        (options.output / (label + '.log')).write_bytes(run.stdout + run.stderr)
        rows.append({'case': label, 'args': command, 'exit': run.returncode})
        print(label, run.returncode)
        assert run.returncode == 0
    headers = [
        ('engine', 'engine/IBaseSystem.h', '/engine/SystemWrapper.cpp', 'hw_engine'),
        ('common', 'common/IBaseSystem.h', '/GameUI/GameUI_Interface.cpp', 'gameui'),
        ('core', 'HLTV/Core/private/common/IBaseSystem.h', '/HLTV/Core/Network.cpp', 'hltv_core'),
        ('demoplayer', 'HLTV/DemoPlayer/private/common/IBaseSystem.h', '/HLTV/DemoPlayer/DemoPlayer.cpp', 'demoplayer')]
    methods = []
    for label, header, suffix, target in headers:
        entry = owner(database, suffix, target)
        source = options.output / ('ibase-' + label + '.cpp')
        source.write_text('#include "' + str(ROOT / 'src' / header) + '''"
#include <type_traits>
template<class...> using ProbeVoid = void;
template<class T, class = void> struct HasPanel : std::false_type {};
template<class T> struct HasPanel<T, ProbeVoid<decltype(&T::GetPanel)>> : std::true_type {};
static_assert(!HasPanel<IBaseSystem>::value, "Native IBaseSystem omits GetPanel");
static_assert(std::is_same<decltype(&IBaseSystem::GetTime), double (IBaseSystem::*)()>::value,
    "Actual time signature");
static_assert(std::is_same<decltype(&IBaseSystem::Printf), void (IBaseSystem::*)(char*, ...)>::value,
    "Actual variadic signature");
''')
        command = prefix(entry) + ['-fsyntax-only', '-Xclang', '-ast-dump=json', str(source)]
        run = subprocess.run(command, cwd=entry['directory'], capture_output=True, timeout=120)
        (options.output / ('ibase-' + label + '.ast.json')).write_bytes(run.stdout)
        (options.output / ('ibase-' + label + '.log')).write_bytes(run.stderr)
        row = {'case': 'ibase-' + label, 'header': header, 'target_profile': target, 'args': command,
               'exit': run.returncode, 'header_sha256': hashlib.sha256((ROOT / 'src' / header).read_bytes()).hexdigest()}
        rows.append(row)
        if run.returncode == 0:
            records = [node for node in walk(json.loads(run.stdout)) if node.get('kind') == 'CXXRecordDecl'
                       and node.get('name') == 'IBaseSystem' and node.get('completeDefinition')]
            assert len(records) == 1
            found = [node for node in records[0].get('inner', []) if node.get('kind') in ['CXXMethodDecl', 'CXXDestructorDecl']
                     and not node.get('isImplicit')]
            methods.append({'header': header, 'bases': records[0].get('bases'), 'declaration_order_not_physical_slots': [
                {'name': method['name'], 'type': method['type']['qualType'], 'virtual': method.get('virtual', False)}
                for method in found]})
        print(row['case'], run.returncode)
    concrete = [
        ('Network', 'HLTV/Core/Network.h', 'INetwork', '/HLTV/Core/Network.cpp', 'hltv_core'),
        ('Server', 'HLTV/Core/Server.h', 'IServer', '/HLTV/Core/Server.cpp', 'hltv_core'),
        ('World', 'HLTV/Core/World.h', 'IWorld', '/HLTV/Core/World.cpp', 'hltv_core'),
        ('Proxy', 'HLTV/Proxy/Proxy.h', 'IProxy', '/HLTV/Proxy/Proxy.cpp', 'hltv_proxy'),
        ('DemoPlayer', 'HLTV/DemoPlayer/DemoPlayer.h', 'IDemoPlayer', '/HLTV/DemoPlayer/DemoPlayer.cpp', 'demoplayer')]
    for name, header, view, suffix, target in concrete:
        entry = owner(database, suffix, target)
        source = options.output / ('view-' + name + '.cpp')
        source.write_text('#include "' + str(ROOT / 'src' / header) + '"\n#include <type_traits>\n'
            + 'static_assert(std::is_convertible<' + name + '*, ISystemModule*>::value, "Actual lifecycle base");\n'
            + 'static_assert(std::is_convertible<' + name + '*, ' + view + '*>::value, "Actual factory view");\n'
            + 'static_assert(sizeof(' + name + ') > 0, "Force actual complete record layout");\n'
            + 'extern \"C\" ' + view + '* ProbeView(' + name + '* value) {\n    return value;\n}\n'
            + 'extern \"C\" ISystemModule* ProbeLifecycle(' + name + '* value) {\n    return value;\n}\n'
            + 'extern \"C\" ISystemModule* ProbeLegacyFactoryCast(' + name + '* value) {\n'
            + '    void* requestedView = static_cast<' + view + '*>(value);\n'
            + '    return (ISystemModule*)requestedView;\n}\n'
            + 'extern \"C\" int ProbeLifecycleVirtualCall(ISystemModule* value) {\n'
            + '    return value->GetSerial();\n}\n')
        command = prefix(entry) + ['-Xclang', '-fdump-record-layouts', '-Xclang', '-fdump-vtable-layouts', '-S', '-emit-llvm', str(source),
                                   '-o', str(options.output / ('view-' + name + '.ll'))]
        run = subprocess.run(command, cwd=entry['directory'], capture_output=True, timeout=120)
        (options.output / ('view-' + name + '.layout.log')).write_bytes(run.stdout + run.stderr)
        rows.append({'case': 'view-' + name, 'header': header, 'view': view, 'args': command,
                     'exit': run.returncode, 'qualification': 'compile-only header probe under actual owner target flags; not a full production TU gate'})
        print('view-' + name, run.returncode)
    (options.output / 'ibase-methods.json').write_text(json.dumps(methods, indent=2))
    (options.output / 'receipt.json').write_text(json.dumps({'database_sha256': hashlib.sha256(options.database.read_bytes()).hexdigest(),
        'rows': rows, 'execution': 'compiler only; no generated binary linked, loaded or executed'}, indent=2))
    if any(row['exit'] != 0 for row in rows):
        raise SystemExit(1)


if __name__ == '__main__':
    main()
