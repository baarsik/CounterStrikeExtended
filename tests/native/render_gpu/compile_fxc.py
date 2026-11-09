"""Offline SDK FXC producer; never creates a GPU/window or logs the environment."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    compiler = Path(r'C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\fxc.exe')
    sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
    expected = '3c1d8f3aa0d1c970732f14d61d3a4b5dfa8ca4b7c81d567643f39f6cef412010'
    assert sha(compiler) == expected, 'Pinned installed SDK FXC identity changed'
    args.output.mkdir(exist_ok=False)
    receipt = {'compiler': str(compiler), 'compiler_sha256': expected,
               'source': str(args.source), 'source_sha256': sha(args.source), 'entries': []}
    for configuration in ['Release', 'Debug']:
        directory = args.output / configuration
        directory.mkdir()
        for entry, profile in [('LegacyVertex', 'vs_5_1'), ('LegacyFragment', 'ps_5_1')]:
            binary = directory / (entry + '.dxbc')
            assembly = directory / (entry + '.assembly.txt')
            command = [str(compiler), '/nologo', '/WX', '/Ges', '/Zpc', '/T', profile, '/E', entry,
                       '/Fo', str(binary), '/Fc', str(assembly)]
            command += ['/Od', '/Zi'] if configuration == 'Debug' else ['/O3']
            command += [str(args.source)]
            result = subprocess.run(command, capture_output=True)
            (directory / (entry + '.compile.log')).write_bytes(result.stdout + result.stderr)
            record = {'configuration': configuration, 'entry': entry, 'profile': profile,
                      'command': command, 'exit': result.returncode}
            if result.returncode == 0:
                record.update(bytes=binary.stat().st_size, sha256=sha(binary), assembly_sha256=sha(assembly))
            receipt['entries'].append(record)
            (args.output / 'compiler-receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
            print(configuration, entry, result.returncode, flush=True)
            if result.returncode:
                print((result.stdout + result.stderr).decode(errors='replace'))
                raise SystemExit(result.returncode)


if __name__ == '__main__':
    main()
