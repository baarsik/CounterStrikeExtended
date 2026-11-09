"""Select actual source branches locally; this does not claim a new whole-module MSVC invocation."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--before', type=Path, required=True)
parser.add_argument('--after', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
receipt = {'qualification': 'Selected Windows source tokens only, not preprocessing transitive headers or compiling a whole module.', 'profiles': []}
for arch in ('x86', 'x64', 'arm64'):
    for policy in ('CSE_ENGINE_WSOCK32', 'CSE_ENGINE_WINSOCK2'):
        options = ['-D_WIN32', '-U__APPLE__', '-D_WIN64' if arch != 'x86' else '-U_WIN64',
                   '-D' + policy, '-U' + ('CSE_ENGINE_WINSOCK2' if policy == 'CSE_ENGINE_WSOCK32' else 'CSE_ENGINE_WSOCK32')]
        variants = {}
        for variant in ('before', 'after'):
            path = getattr(args, variant)
            result = subprocess.run(['unifdef', *options, str(path)], capture_output=True)
            assert result.returncode in (0, 1), result.stderr
            lexical = rb'//[^\r\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\\r\n])*"|' + \
                      rb"'(?:\\.|[^'\\\r\n])*'|[A-Za-z_]\w*|[0-9]+|[^\s]"
            tokens = [token for token in re.findall(lexical, result.stdout)
                      if not token.startswith((b'//', b'/*'))]
            variants[variant] = {'count': len(tokens), 'sha256': hashlib.sha256(b'\0'.join(tokens)).hexdigest()}
        assert variants['before'] == variants['after'], (arch, policy, variants)
        receipt['profiles'].append({'architecture': arch, 'socket_policy': policy, **variants})
receipt['input_hashes'] = {name: hashlib.sha256(getattr(args, name).read_bytes()).hexdigest() for name in ('before', 'after')}
args.output.write_text(json.dumps(receipt, indent=2) + '\n')
