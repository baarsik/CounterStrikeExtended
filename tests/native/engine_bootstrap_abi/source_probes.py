#!/usr/bin/env python3
"""Compile real owner/caller TUs to record bootstrap type, slot and call facts."""
import argparse
import ctypes
import hashlib
import json
import os
import re
from pathlib import Path
import shlex
import shutil
import subprocess


ENGINE_CALL = '''
extern "C" int BootstrapProbeRun(IEngineAPI* api, void* instance, char* base, char* command, char* restart,
    CreateInterfaceFn launcher, CreateInterfaceFn filesystem) {
    return api->Run(instance, base, command, restart, launcher, filesystem);
}
extern "C" IBaseInterface* BootstrapProbeEngineBase(IEngineAPI* api) {
    return api;
}
'''

DEDICATED_CALLS = '''
extern "C" bool BootstrapProbeInit(IDedicatedServerAPI* api, char* base, char* command,
    CreateInterfaceFn launcher, CreateInterfaceFn filesystem) {
    return api->Init(base, command, launcher, filesystem);
}
extern "C" int BootstrapProbeShutdown(IDedicatedServerAPI* api) {
    return api->Shutdown();
}
extern "C" bool BootstrapProbeFrame(IDedicatedServerAPI* api) {
    return api->RunFrame();
}
extern "C" void BootstrapProbeConsole(IDedicatedServerAPI* api, char* text) {
    api->AddConsoleText(text);
}
extern "C" void BootstrapProbeStatus(IDedicatedServerAPI* api, float* fps, int* active, int* maximum, char* map) {
    api->UpdateStatus(fps, active, maximum, map);
}
extern "C" void BootstrapProbePrint(IDedicatedExports* api, char* text) {
    api->Sys_Printf(text);
}
extern "C" IBaseInterface* BootstrapProbeDedicatedBase(IDedicatedServerAPI* api) {
    return api;
}
extern "C" IBaseInterface* BootstrapProbeExportsBase(IDedicatedExports* api) {
    return api;
}
'''

FACTORY_CALLS = '''
extern "C" void* BootstrapProbeFactory(CreateInterfaceFn factory, const char* name, int* status) {
    return factory(name, status);
}
extern "C" void BootstrapProbeFilesystem(IFileSystem* filesystem) {
    filesystem->Mount();
}
'''


def windows_args(command):
    shell = ctypes.windll.shell32
    shell.CommandLineToArgvW.argtypes = [ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_int)]
    shell.CommandLineToArgvW.restype = ctypes.POINTER(ctypes.c_wchar_p)
    count = ctypes.c_int()
    pointer = shell.CommandLineToArgvW(command, ctypes.byref(count))
    if not pointer:
        raise ctypes.WinError()
    try:
        return [pointer[index] for index in range(count.value)]
    finally:
        ctypes.windll.kernel32.LocalFree(ctypes.cast(pointer, ctypes.c_void_p))


def target_of(row):
    match = re.search(r'(?:^|/)targets/([^/]+)/', row['output'].replace('\\', '/'))
    if not match:
        raise ValueError('Not a retained target recipe: ' + row['output'])
    return match.group(1)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--database', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--target', action='append')
    parser.add_argument('--archive', type=Path)
    parser.add_argument('--compiler-target', choices=['x86', 'x64', 'arm64'])
    parser.add_argument('--contract-header', type=Path)
    options = parser.parse_args()
    options.output.mkdir(parents=True, exist_ok=True)
    database = json.loads(options.database.read_text())
    rows = []
    for entry in database:
        if not re.search(r'(?:^|/)targets/([^/]+)/', entry['output'].replace('\\', '/')):
            continue
        target = target_of(entry)
        suffix = entry['file'].replace('\\', '/')
        if target not in (options.target or ['hw_engine', 'swds', 'launcher', 'hlds']):
            continue
        if not any(suffix.endswith(name) for name in ['/engine/sys_dll2.cpp', '/launcher/launcher.cpp',
                                                       '/dedicated/sys_ded.cpp']):
            continue
        source = options.output / (target + '.cpp')
        text = '#include "' + suffix + '"\n'
        if target in ['hw_engine', 'swds', 'launcher']:
            text += ENGINE_CALL
        if target in ['hw_engine', 'swds', 'hlds']:
            text += DEDICATED_CALLS
        text += FACTORY_CALLS
        if options.contract_header:
            text += '#include "' + str(options.contract_header).replace('\\', '/') + '"\n'
            text += 'namespace BootstrapContract = cse::engine_bootstrap_abi;\n'
            if target in ['hw_engine', 'swds']:
                text += ('constexpr auto BootstrapDescription = BootstrapContract::DescribeEngine<IEngineAPI, '
                         'IDedicatedServerAPI, IDedicatedExports, CreateInterfaceFn>();\n')
            elif target == 'launcher':
                text += ('constexpr auto BootstrapDescription = BootstrapContract::DescribeLauncher<IEngineAPI, '
                         'CreateInterfaceFn>();\n')
            else:
                text += ('constexpr auto BootstrapDescription = BootstrapContract::DescribeDedicatedHost<'
                         'IDedicatedServerAPI, IDedicatedExports, CreateInterfaceFn>();\n')
            text += 'static_assert(BootstrapDescription.tableCount == 3, "Exact bounded bootstrap catalog");\n'
        source.write_text(text)
        if os.name == 'nt':
            args = windows_args(entry['command'])
            if options.compiler_target:
                args[0] = str(Path(args[0]).parent.parent / options.compiler_target / 'cl.exe')
            args = [arg for arg in args if not arg.lower().startswith(('/fo', '/fd'))
                    and arg.replace('\\', '/') != suffix]
            command = args + ['/d1reportAllClassLayout']
            command += ['/Fo' + str(options.output / (target + '.obj')),
                        '/Fd' + str(options.output / (target + '.pdb')), str(source)]
        else:
            args = shlex.split(entry['command'])
            command = args[:args.index('-o')] + ['-Xclang', '-fdump-vtable-layouts', '-Xclang',
                '-fdump-record-layouts', '-S', '-emit-llvm', str(source),
                '-o', str(options.output / (target + '.ll'))]
        run = subprocess.run(command, cwd=options.output, capture_output=True, timeout=180)
        log = options.output / (target + '.log')
        log.write_bytes(run.stdout + run.stderr)
        rows.append({'target': target, 'args': command, 'exit': run.returncode,
                     'owner_recipe': entry, 'source_sha256': hashlib.sha256(Path(entry['file']).read_bytes()).hexdigest(),
                     'qualification': 'actual production CPP included unchanged; compile only, no link or execution',
                     'compiler_target_override': options.compiler_target})
        print(target, run.returncode, flush=True)
    assert {row['target'] for row in rows} == set(options.target or ['hw_engine', 'swds', 'launcher', 'hlds'])
    (options.output / 'receipt.json').write_text(json.dumps(rows, indent=2))
    if options.archive:
        options.archive.mkdir(parents=True, exist_ok=True)
        for path in options.output.iterdir():
            if path.suffix in ['.cpp', '.ll', '.log', '.json']:
                shutil.copyfile(path, options.archive / path.name)
        if os.name == 'nt':
            compilers = sorted(set(row['args'][0] for row in rows))
            (options.archive / 'compiler-identity.json').write_text(json.dumps([
                {'path': name, 'sha256': hashlib.sha256(Path(name).read_bytes()).hexdigest()}
                for name in compilers], indent=2))


if __name__ == '__main__':
    main()
