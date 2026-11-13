"""Pure CPU fixture plus immutable V17 recipe compile/token controls; no runtime provider."""
from pathlib import Path
import argparse
import ctypes
import hashlib
import json
import shutil
import struct
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--inputs', required=True, type=Path)
parser.add_argument('--workspace', required=True, type=Path)
parser.add_argument('--output', required=True, type=Path)
args = parser.parse_args()
assert not args.workspace.exists()
args.workspace.mkdir()
args.output.mkdir()
source = args.workspace / 'source'
shutil.copytree(args.inputs / 'source-final', source)
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
hashes = json.loads((args.inputs / 'source-final-hashes.json').read_text())
assert all(sha(source / name) == digest for name, digest in hashes.items())
before = source / 'src/engine/VGUI_EngineSurfaceWrap-before.cpp'
shutil.copy2(args.inputs / 'before/VGUI_EngineSurfaceWrap.cpp', before)
after = source / 'src/engine/VGUI_EngineSurfaceWrap.cpp'
shell = ctypes.WinDLL('shell32', use_last_error=True)
shell.CommandLineToArgvW.argtypes = [ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_int)]
shell.CommandLineToArgvW.restype = ctypes.POINTER(ctypes.c_wchar_p)
kernel = ctypes.WinDLL('kernel32', use_last_error=True)
kernel.LocalFree.argtypes = [ctypes.c_void_p]


def command_argv(command):
    count = ctypes.c_int()
    parsed = shell.CommandLineToArgvW(command, ctypes.byref(count))
    assert parsed
    values = [parsed[i] for i in range(count.value)]
    kernel.LocalFree(parsed)
    return values


def execute(command, output, environment, cwd=None):
    with output.open('wb') as log:
        result = subprocess.run(command, env=environment, cwd=cwd, stdout=log,
                                stderr=subprocess.STDOUT, timeout=120)
    return result.returncode


rows = []
owners = []
for arch, machine in [('x86', 0x14c), ('x64', 0x8664), ('arm64', 0xaa64)]:
    initialized = subprocess.run(['cmd.exe', '/d', '/u', '/c',
        r'call C:\BuildTools\2026\Common7\Tools\VsDevCmd.bat -no_logo -host_arch=arm64 -arch=' + arch + ' && set'],
        capture_output=True, check=True)
    environment = {}
    for line in initialized.stdout.decode('utf-16-le').splitlines():
        key, separator, value = line.partition('=')
        if key and separator:
            environment[key] = value
    assert environment['VSCMD_ARG_TGT_ARCH'] == arch
    path = next(value for key, value in environment.items() if key.upper() == 'PATH')
    compiler = Path(shutil.which('cl.exe', path=path))
    ninja = Path(shutil.which('ninja.exe', path=path))
    dumpbin = Path(shutil.which('dumpbin.exe', path=path))
    execute([str(compiler)], args.output / (arch + '-compiler.log'), environment)
    for config in ['Release', 'Debug']:
        label = arch + '-' + config
        output = args.output / label
        output.mkdir()
        build = args.workspace / label
        row = {'architecture': arch, 'configuration': config, 'compiler': str(compiler),
               'compiler-sha256': sha(compiler), 'phases': [], 'runtime': []}
        commands = [('configure', ['cmake', '-S', str(source / 'tests/native/vgui1_engine_font_atlas'),
            '-B', str(build), '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=' + config]),
            ('build', ['cmake', '--build', str(build), '--parallel', '1'])]
        for phase, command in commands:
            code = execute(command, output / (phase + '.log'), environment)
            row['phases'].append({'phase': phase, 'command': command, 'exit': code})
            if code:
                break
        if len(row['phases']) == 2 and row['phases'][-1]['exit'] == 0:
            image = build / 'atlas_methods.exe'
            data = image.read_bytes()
            pe = struct.unpack_from('<I', data, 0x3c)[0]
            assert struct.unpack_from('<H', data, pe + 4)[0] == machine
            row['image-sha256'] = sha(image)
            row['pe-machine'] = hex(machine)
            shutil.copy2(image, output / image.name)
            execute([str(dumpbin), '/imports', str(image)], output / 'imports.log', environment)
            shutil.copy2(build / 'compile_commands.json', output / 'compile_commands.json')
            shutil.copytree(build / 'generated', output / 'generated')
            for mode in ['--admission', '--width', '--core', '--capacity-diagnostic']:
                code = execute([str(image), mode], output / (mode[2:] + '.log'), environment)
                row['runtime'].append({'mode': mode, 'exit': code, 'expected-exit': 78 if 'capacity' in mode else 0})
        rows.append(row)
        (args.output / 'component-cases.json').write_text(json.dumps(rows, indent=2) + '\n')
        print(label, [x['exit'] for x in row['phases']], row['runtime'], flush=True)

        if arch == 'arm64':
            continue  # No frozen V17 ARM64 producer. Do not invent a recipe.
        preset = 'runtime-windows-x64-' + config.lower() if arch == 'x64' else config.lower()
        producer = Path(r'C:\CI\CSE-Native64-RuntimeV17-20261109\source\build') / preset
        database = json.loads(subprocess.check_output([str(ninja), '-C', str(producer), '-t', 'compdb', '-x'],
                                                     env=environment))
        entries = [entry for entry in database
                   if entry['file'].replace('\\', '/').endswith('/VGUI_EngineSurfaceWrap.cpp')]
        assert len(entries) == 2, len(entries)
        for entry in entries:
            target = 'swds' if 'targets/swds/' in entry['output'].replace('\\', '/') else 'hw_engine'
            owner_output = output / target
            owner_output.mkdir()
            original = command_argv(entry['command'])
            assert not any(x.lower().startswith(('/yc', '/yu')) for x in original)
            compile_args = []
            for value in original:
                if value.lower().startswith('/fo'):
                    value = '/Fo' + str(args.workspace / (label + '-' + target + '.obj'))
                elif value.lower().startswith('/fd'):
                    value = '/Fd' + str(args.workspace / (label + '-' + target + '-compile.pdb'))
                elif value.replace('\\', '/').casefold() == entry['file'].replace('\\', '/').casefold():
                    value = str(after)
                compile_args.append(value)
            assert str(after) in compile_args
            code = execute(compile_args, owner_output / 'compile.log', environment, entry['directory'])
            owner = {'architecture': arch, 'configuration': config, 'owner': target,
                     'producer': str(producer), 'original-command': entry['command'],
                     'command': compile_args, 'exit': code, 'tokens': []}
            if code == 0:
                obj = args.workspace / (label + '-' + target + '.obj')
                shutil.copy2(obj, owner_output / 'owner.obj')
                owner['object-sha256'] = sha(obj)
                owner['ltcg'] = any(x.lower() == '/gl' for x in compile_args)
                execute([str(dumpbin), '/headers', str(obj)], owner_output / 'object-headers.log', environment)
            for version, cpp in [('before', before), ('after', after)]:
                pre_args = []
                for value in compile_args:
                    if value.lower().startswith(('/fo', '/fd', '/showincludes')) or value.lower() in ['/c', '-c']:
                        continue
                    pre_args.append(str(cpp) if value == str(after) else value)
                pre_args += ['/EP']
                pre_file = owner_output / (version + '.i')
                pre_code = execute(pre_args, pre_file, environment, entry['directory'])
                owner['tokens'].append({'version': version, 'command': pre_args, 'exit': pre_code})
            if all(x['exit'] == 0 for x in owner['tokens']):
                texts = []
                for version, cpp in [('before', before), ('after', after)]:
                    data = (owner_output / (version + '.i')).read_bytes()
                    lines = data.splitlines(keepends=True)
                    assert lines[0].strip() == cpp.name.encode(), 'Expected cl input basename banner'
                    data = b''.join(lines[1:])
                    for name in [str(cpp), str(cpp).replace('\\', '\\\\')]:
                        data = data.replace(name.encode(), b'<owned-source>')
                    texts.append(data)
                owner['normalization'] = 'Remove exact compiler first-line input basename banner and canonicalize only source path literals'
                owner['whole-preprocessed-selected-code-identical'] = texts[0] == texts[1]
                owner['normalized-preprocessed-sha256'] = [hashlib.sha256(x).hexdigest() for x in texts]
            owners.append(owner)
            (args.output / 'owning-cases.json').write_text(json.dumps(owners, indent=2) + '\n')
            print(label, target, code, owner.get('whole-preprocessed-selected-code-identical'), flush=True)
assert all(sha(source / name) == digest for name, digest in hashes.items())
ok = len(rows) == 6 and all(len(x['phases']) == 2 and x['phases'][-1]['exit'] == 0
    and len(x['runtime']) == 4 and all(y['exit'] == y['expected-exit'] for y in x['runtime']) for x in rows)
ok = ok and len(owners) == 8 and all(x['exit'] == 0 and x.get('whole-preprocessed-selected-code-identical') for x in owners)
raise SystemExit(int(not ok))
