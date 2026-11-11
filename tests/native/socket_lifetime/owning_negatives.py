"""Mutate copied real declarations/initializers under an actual owning recipe; no production edits."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shlex
import subprocess

root = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser()
parser.add_argument('database', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
rows = json.loads(args.database.read_text())
spec = importlib.util.spec_from_file_location('socket_evidence', root / 'tools/socket_abi_evidence.py')
evidence = importlib.util.module_from_spec(spec)
spec.loader.exec_module(evidence)
header = (root / 'GoldSrc-SDK/public/native_socket_host.h').read_text()
mutations = {
    'binding-role-order': ('header', '    OwnsFn owns;\n    CreateUdpFn createUdp;',
                          '    CreateUdpFn createUdp;\n    OwnsFn owns;'),
    'descriptor-width': ('header', 'std::int32_t descriptor;', 'std::int64_t descriptor;'),
    'ownership-signature': ('header', 'using OwnsFn = bool (*)(void* context,',
                           'using OwnsFn = bool (*)(const void* context,'),
    'canonical-initializer': ('owner', 'HostOwns, HostCreateUdp, HostCloseUdp, HostQuiescent};',
                            'HostQuiescent, HostCreateUdp, HostCloseUdp, HostOwns};')}
receipts = []
for role in ['hw_engine', 'swds', 'launcher', 'hlds']:
    is_host = role in ['launcher', 'hlds']
    relative = Path('launcher/native/SocketImageOwner.cpp' if is_host else 'engine/native_socket_lifetime.cpp')
    row = next(row for row in rows if row['file'].endswith('/' + str(relative))
               and 'targets/' + role + '/' in row['command'])
    clean = []
    skip = False
    for item in shlex.split(row['command']):
        if skip:
            skip = False
        elif item in ['-o', '-MF', '-MT', '-MQ']:
            skip = True
        elif item not in ['-c', '-MD', '-MMD', row['file']]:
            clean.append(item)
    owner = (root / 'GoldSrc-SDK' / relative).read_text()
    owner_header = (root / 'GoldSrc-SDK' / relative.with_suffix('.h')).read_text()
    for name, (kind, before, after) in mutations.items():
        if kind == 'owner' and not is_host:
            continue
        directory = args.output / role / name
        public = directory / 'GoldSrc-SDK/public'
        native = directory / 'GoldSrc-SDK' / relative.parent
        public.mkdir(parents=True, exist_ok=True)
        native.mkdir(parents=True, exist_ok=True)
        assert before in (header if kind == 'header' else owner)
        (public / 'native_socket_host.h').write_text(header.replace(before, after, 1) if kind == 'header' else header)
        (native / relative.with_suffix('.h').name).write_text(owner_header)
        source = native / relative.name
        source.write_text((owner.replace(before, after, 1) if kind == 'owner' else owner) + evidence.PROBE)
        command = clean + ['-fsyntax-only', str(source)]
        result = subprocess.run(command, cwd=row['directory'], capture_output=True)
        log = directory / 'compile.log'
        log.write_bytes(result.stdout + result.stderr)
        if result.returncode == 0:
            raise RuntimeError(role + '/' + name + ': corrupted actual declaration/role unexpectedly compiled')
        receipts.append(dict(role=role, case=name, command=command, exit=result.returncode,
                             log_sha256=hashlib.sha256(log.read_bytes()).hexdigest(),
                             source_sha256=hashlib.sha256(source.read_bytes()).hexdigest()))
(args.output / 'receipt.json').write_text(json.dumps(receipts, indent=2) + '\n')
print('Rejected real owning declaration/initializer mutations:', len(receipts))
