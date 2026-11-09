#!/usr/bin/env python3
"""Compile pinned real declarations where the full POSIX owner TU is still blocked."""
import argparse
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess

from source_probes import ENGINE_CALL, DEDICATED_CALLS, FACTORY_CALLS, windows_args


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--database', type=Path, required=True)
    parser.add_argument('--bindings', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--fault', choices=['none', 'reorder', 'insert'], default='none')
    parser.add_argument('--header', type=Path)
    parser.add_argument('--archive', type=Path)
    parser.add_argument('--compiler-target', choices=['x86', 'x64', 'arm64'])
    options = parser.parse_args()
    options.output.mkdir(parents=True, exist_ok=True)
    database = json.loads(options.database.read_text())
    bindings = options.bindings.read_text()
    if options.fault == 'reorder':
        original = '\tvirtual int Shutdown() = 0;\n\tvirtual bool RunFrame() = 0;'
        replacement = '\tvirtual bool RunFrame() = 0;\n\tvirtual int Shutdown() = 0;'
        assert bindings.count(original) == 2
        bindings = bindings.replace(original, replacement)
    elif options.fault == 'insert':
        original = 'class IDedicatedServerAPI : public IBaseInterface\n{\npublic:\n'
        assert bindings.count(original) == 2
        bindings = bindings.replace(original, original + '\tvirtual void ReservedForNegativeControl() = 0;\n')
    bound = options.output / 'actual_declarations.h'
    bound.write_text(bindings)
    contexts = [
        ('producer', 'hw_engine', '/engine/sys_dll2.cpp', 'using namespace EngineProducer;\n',
         ENGINE_CALL + DEDICATED_CALLS,
         'DescribeEngine<IEngineAPI, IDedicatedServerAPI, IDedicatedExports, CreateInterfaceFn>()'),
        ('launcher', 'launcher', '/launcher/launcher.cpp', 'using namespace LauncherCaller;\n', ENGINE_CALL,
         'DescribeLauncher<IEngineAPI, CreateInterfaceFn>()'),
        ('dedicated', 'hlds', '/dedicated/sys_ded.cpp',
         'using namespace DedicatedCaller;\nusing namespace DedicatedExportsCaller;\n', DEDICATED_CALLS,
         'DescribeDedicatedHost<IDedicatedServerAPI, IDedicatedExports, CreateInterfaceFn>()')]
    rows = []
    for label, target, suffix, aliases, calls, describe in contexts:
        entry = next(row for row in database if row['file'].endswith(suffix)
                     and re.search(r'(?:^|/)targets/' + target + '/', row['output']))
        args = windows_args(entry['command']) if os.name == 'nt' else shlex.split(entry['command'])
        source = options.output / (label + '.cpp')
        header = options.header or options.root / 'GoldSrc-SDK/public/cse_engine_bootstrap_abi.h'
        selected_namespaces = {
            'producer': ['EngineProducer'], 'launcher': ['LauncherCaller'],
            'dedicated': ['DedicatedCaller', 'DedicatedExportsCaller']}[label]
        context_bindings = bindings[:bindings.index('namespace ')]
        for namespace in selected_namespaces:
            match = re.search(r'namespace ' + namespace + r' \{\n.*?\n\}(?=\n|$)', bindings, re.S)
            assert match, namespace
            context_bindings += match.group(0) + '\n'
        assert context_bindings.count('{') == context_bindings.count('}'), label
        context_header = options.output / (label + '-bindings.h')
        context_header.write_text(context_bindings)
        source.write_text('#include "' + str(options.root / 'GoldSrc-SDK/public/FileSystem.h') + '"\n'
                          + '#include "' + str(context_header) + '"\n'
                          + '#include "' + header.as_posix() + '"\n'
                          + aliases + calls + FACTORY_CALLS
                          + 'constexpr auto Description = cse::engine_bootstrap_abi::' + describe + ';\n'
                          + 'static_assert(Description.tableCount == 3, "Bounded bootstrap group");\n')
        if label != 'launcher':
            with source.open('a') as stream:
                stream.write('static_assert(sizeof(IDedicatedServerAPI) == sizeof(void*), '
                             '"Same-size virtual order/insert controls");\n')
        if os.name == 'nt':
            if options.compiler_target:
                args[0] = str(Path(args[0]).parent.parent / options.compiler_target / 'cl.exe')
            actual_source = entry['file'].replace('\\', '/')
            args = [argument for argument in args if not argument.lower().startswith(('/fo', '/fd'))
                    and argument.replace('\\', '/') != actual_source]
            command = args + ['/d1reportAllClassLayout', '/Fo' + str(options.output / (label + '.obj')),
                              '/Fd' + str(options.output / (label + '.pdb')), str(source)]
        else:
            command = args[:args.index('-o')] + ['-Xclang', '-fdump-vtable-layouts', '-Xclang', '-fdump-record-layouts',
                                                '-S', '-emit-llvm', str(source),
                                                '-o', str(options.output / (label + '.ll'))]
        run = subprocess.run(command, capture_output=True, timeout=120)
        (options.output / (label + '.log')).write_bytes(run.stdout + run.stderr)
        rows.append({'case': label, 'args': command, 'exit': run.returncode, 'fault': options.fault,
                     'owner_recipe': entry,
                     'qualification': 'exact source-extracted declarations with actual base/FS headers; controlled bindings, not a whole-TU gate'})
        print(label, run.returncode)
    (options.output / 'receipt.json').write_text(json.dumps(rows, indent=2))
    if options.archive:
        options.archive.mkdir(parents=True, exist_ok=True)
        for path in options.output.iterdir():
            if path.is_file() and path.suffix in ['.cpp', '.ll', '.log', '.json', '.h']:
                shutil.copyfile(path, options.archive / path.name)
    if any(row['exit'] for row in rows):
        raise SystemExit(1)


if __name__ == '__main__':
    main()
