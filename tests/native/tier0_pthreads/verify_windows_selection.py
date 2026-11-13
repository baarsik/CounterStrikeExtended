"""Check this POSIX-only fix against the exact already-built Windows source/header inputs."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


def digest(data):
    return hashlib.sha256(data).hexdigest()


def run(args):
    relative = Path('src/tier0/threadtools.cpp')
    before = args.baseline / relative
    after = args.current / relative
    profiles = {}
    for arch, native in [('x86', False), ('x64', True), ('arm64', True)]:
        options = ['-D_WIN32', '-U__APPLE__', '-D_WIN64' if native else '-U_WIN64',
                   '-D__LP64__' if native else '-U__LP64__']
        variants = {}
        for name, path in [('before', before), ('after', after)]:
            result = subprocess.run(['unifdef', *options, str(path)], capture_output=True)
            assert result.returncode in (0, 1), result.stderr
            tokens = re.findall(rb'[A-Za-z_]\w*|[0-9]+|"(?:\\.|[^"\\])*"|'
                                rb"\x27(?:\\.|[^\x27\\])*\x27|[^\s]", result.stdout)
            variants[name] = {'count': len(tokens), 'tokens_sha256': digest(b'\0'.join(tokens)),
                              'source_sha256': digest(result.stdout)}
        assert variants['before'] == variants['after'], arch
        profiles[arch] = variants
    headers = {}
    for name in ['threadtools.h', 'platform.h', 'dbg.h']:
        header = Path('src/public/tier0') / name
        data = (args.baseline / header).read_bytes()
        assert data == (args.current / header).read_bytes(), header
        headers[header.as_posix()] = digest(data)
    args.output.write_text(json.dumps({'windows_selected_source': profiles, 'unchanged_project_headers': headers,
        'baseline_cpp_sha256': digest(before.read_bytes()),
        'qualification': 'Local source selection and token invariance only; no new MSVC invocation.'}, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--current', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    run(parser.parse_args())
