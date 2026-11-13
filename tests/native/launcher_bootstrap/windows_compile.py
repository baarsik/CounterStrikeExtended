"""Headless MSVC compile/token gate using a frozen historical reference inventory receipt."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess


def digest(data):
    return hashlib.sha256(data).hexdigest()


def run(args):
    manifest = json.loads((args.inputs / 'manifest.json').read_text())
    source = args.workspace / 'source'
    if source.exists():
        raise RuntimeError('The physical source workspace must be new')
    shutil.copytree(args.inputs / 'source', source)
    for name, expected in manifest['hashes'].items():
        assert digest((source / name).read_bytes()) == expected, name
    receipt = {'inputs': manifest, 'cases': []}
    launcher = source / 'src/launcher/launcher.cpp'
    vs = r'C:\BuildTools\2026\Common7\Tools\VsDevCmd.bat'
    for arch, machine in [('x86', 0x14c), ('x64', 0x8664), ('arm64', 0xaa64)]:
        initialized = subprocess.run(['cmd.exe', '/d', '/u', '/c',
            f'call {vs} -no_logo -host_arch=arm64 -arch={arch} && set'],
            capture_output=True, check=True)
        environment = {}
        for line in initialized.stdout.decode('utf-16-le').splitlines():
            key, separator, value = line.partition('=')
            if separator and key:
                environment[key] = value
        assert environment.get('VSCMD_ARG_TGT_ARCH') == arch
        compiler = shutil.which('cl.exe', path=environment.get('Path', environment.get('PATH')))
        assert compiler
        for config in ['Release', 'Debug']:
            case = {'architecture': arch, 'configuration': config, 'tokens': {}, 'objects': []}
            output = args.workspace / (arch + '-' + config)
            output.mkdir()
            for variant in ['before', 'after']:
                launcher.write_bytes((args.inputs / ('launcher-' + variant + '.cpp')).read_bytes())
                definitions = manifest['configurations'][config][0]['metadata']['PreprocessorDefinitions']
                flags = ['/nologo', '/std:c++14', '/EHsc', '/W4', '/WX', '/permissive-', '/sdl',
                         '/MDd' if config == 'Debug' else '/MD', '/Od' if config == 'Debug' else '/O2']
                flags += ['/D' + value for value in definitions.split(';') if value]
                tokens_command = [compiler] + flags + ['/EP', str(launcher)]
                preprocessed = subprocess.run(tokens_command, env=environment, capture_output=True)
                (output / (variant + '-preprocess.stderr')).write_bytes(preprocessed.stderr)
                assert preprocessed.returncode == 0, (arch, config, variant, 'preprocess')
                (output / (variant + '.i')).write_bytes(preprocessed.stdout)
                tokens = re.findall(rb'[A-Za-z_]\w*|[0-9]+|"(?:\\.|[^"\\])*"|'
                                    rb"\x27(?:\\.|[^\x27\\])*\x27|[^\s]", preprocessed.stdout)
                case['tokens'][variant] = {'count': len(tokens), 'sha256': digest(b'\0'.join(tokens))}
            assert case['tokens']['before'] == case['tokens']['after'], case
            for index, item in enumerate(manifest['configurations'][config]):
                obj = output / (str(index) + '.obj')
                command = [compiler] + flags + ['/c', str(source / item['relative']), '/Fo' + str(obj)]
                completed = subprocess.run(command, env=environment, capture_output=True)
                (output / (str(index) + '-compile.log')).write_bytes(completed.stdout + completed.stderr)
                assert completed.returncode == 0, (arch, config, item['relative'], 'compile')
                data = obj.read_bytes()
                observed = struct.unpack_from('<H', data)[0]
                assert observed == machine, (arch, hex(observed))
                case['objects'].append({'source': item['relative'], 'sha256': digest(data),
                                        'machine': hex(observed)})
            receipt['cases'].append(case)
            (args.receipts / 'windows-compile.json').write_text(json.dumps(receipt, indent=2) + '\n')
            print(arch, config, 'tokens identical; four actual launcher translation units compiled', flush=True)
    assert digest(launcher.read_bytes()) == manifest['hashes']['src/launcher/launcher.cpp']


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--inputs', type=Path, required=True)
    parser.add_argument('--workspace', type=Path, required=True)
    parser.add_argument('--receipts', type=Path, required=True)
    run(parser.parse_args())
