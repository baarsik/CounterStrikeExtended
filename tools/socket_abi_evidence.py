#!/usr/bin/env python3
"""Mandatory actual HW/SW/executable socket contract owning compilation evidence."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
RECORDS = {
    'CreateResult': (8, 4, [('descriptor', 0), ('nativeError', 4)]),
    'CloseResult': (8, 4, [('disposition', 0), ('nativeError', 4)]),
    'Binding': (72, 8, [('size', 0), ('version', 4), ('epoch', 8), ('imageBase', 16), ('context', 24),
                      ('originPID', 32), ('reserved', 36), ('owns', 40), ('createUdp', 48),
                      ('closeUdp', 56), ('quiescent', 64)]),
    'API': (32, 8, [('size', 0), ('version', 4), ('contract', 8), ('bind', 16), ('retire', 24)]),
    'HostTable': (48, 8, [('size', 0), ('version', 4), ('contract', 8), ('owns', 16),
                        ('createUdp', 24), ('closeUdp', 32), ('quiescent', 40)])}
PROBE = '''
using namespace cse::socket_lifetime;
static_assert(std::is_same_v<OwnsFn, bool (*)(void*, const void*, std::uint64_t) noexcept>);
static_assert(std::is_same_v<CreateUdpFn, Status (*)(void*, const void*, std::uint64_t, CreateResult*) noexcept>);
static_assert(std::is_same_v<CloseUdpFn, Status (*)(void*, const void*, std::uint64_t, std::int32_t, CloseResult*) noexcept>);
static_assert(std::is_same_v<QuiescentFn, Status (*)(void*, const void*, std::uint64_t) noexcept>);
static_assert(std::is_same_v<decltype(CreateResult::descriptor), std::int32_t>);
static_assert(std::is_same_v<decltype(CreateResult::nativeError), std::int32_t>);
static_assert(std::is_same_v<decltype(CloseResult::disposition), CloseDisposition>);
static_assert(std::is_same_v<decltype(CloseResult::nativeError), std::int32_t>);
static_assert(std::is_same_v<decltype(Binding::originPID), std::int32_t>);
static_assert(std::is_same_v<decltype(Binding::epoch), std::uint64_t>);
static_assert(static_cast<std::uint32_t>(Status::Retired) == 1 && static_cast<std::uint32_t>(Status::Invalid) == 2);
static_assert(static_cast<std::uint32_t>(Status::WrongProcess) == 3 && static_cast<std::uint32_t>(Status::WrongThread) == 4);
static_assert(static_cast<std::uint32_t>(Status::Busy) == 5 && static_cast<std::uint32_t>(Status::LiveSockets) == 6);
static_assert(static_cast<std::uint32_t>(Status::NativeFailure) == 7);
static_assert(static_cast<std::uint32_t>(CloseDisposition::NotAttempted) == 0 &&
              static_cast<std::uint32_t>(CloseDisposition::Retained) == 1 &&
              static_cast<std::uint32_t>(CloseDisposition::Consumed) == 2);
static_assert(Version == 1 && Contract == 0x435345534f434b31ull && HostContract == 0x435345534f434831ull);
static_assert(static_cast<std::uint32_t>(Status::Ready) == 0 && static_cast<std::uint32_t>(Status::MissingOwner) == 8);
'''


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def verify_records(log):
    for name, (size, alignment, fields) in RECORDS.items():
        record = next((part.split('\n\n')[0] for part in log.split('*** Dumping AST Record Layout\n')
                       if re.match(r'\s*0 \| struct cse::socket_lifetime::' + name + r'\n', part)), None)
        if not record or not re.search(r'\[sizeof=' + str(size) + r',.*?align=' + str(alignment) + r',', record, re.S):
            raise RuntimeError(f'Wrong/missing physical record {name}')
        for field, offset in fields:
            if not re.search(r'^\s*' + str(offset) + r' \|   [^\n]*\b' + field + r'$', record, re.M):
                raise RuntimeError(f'Wrong/missing physical field {name}.{field} at {offset}')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--database', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--roles', nargs='+', default=['hw_engine', 'swds', 'launcher', 'hlds'])
    options = parser.parse_args()
    options.output.mkdir(parents=True, exist_ok=True)
    stamp = options.output / 'accepted.json'
    stamp.unlink(missing_ok=True)
    rows = json.loads(options.database.read_text())
    receipts = []
    for role in options.roles:
        host = role in ['launcher', 'hlds']
        suffix = '/launcher/native/SocketImageOwner.cpp' if host else '/engine/native_socket_lifetime.cpp'
        matches = [row for row in rows if row['file'].endswith(suffix) and f'targets/{role}/' in row['command']]
        if len(matches) != 1:
            raise RuntimeError(f'{role}: expected actual single owning recipe, got {len(matches)}')
        row = matches[0]
        source = Path(row['file'])
        probe = options.output / (role + '.cpp')
        probe.write_text('#include "' + str(source) + '"\n' + PROBE)
        args = shlex.split(row['command'])
        clean = []
        skip = False
        for argument in args:
            if skip:
                skip = False
            elif argument in ['-o', '-MF', '-MT', '-MQ']:
                skip = True
            elif argument.startswith('@'):
                raise RuntimeError('Unqualified response file owning policy')
            elif argument not in ['-MD', '-MMD', '-c', row['file']]:
                clean.append(argument)
        command = clean + ['-Xclang', '-fdump-record-layouts', '-fsyntax-only', str(probe)]
        result = subprocess.run(command, cwd=row['directory'], capture_output=True, timeout=180)
        log = options.output / (role + '.log')
        log.write_bytes(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(f'{role}: actual owning signature/initializer compile failed: {log}')
        verify_records(log.read_text())
        preprocess = subprocess.run(clean + ['-E', '-P', str(probe)], cwd=row['directory'],
                                    capture_output=True, check=True, timeout=180).stdout
        if host:
            normalized = re.sub(r'\s+', '', preprocess.decode())
            initializer = 'constexprHostTableCanonicalHost{sizeof(HostTable),Version,HostContract,HostOwns,HostCreateUdp,HostCloseUdp,HostQuiescent};'
            if initializer not in normalized or 'CseNativeSocketHostV1=cse::socket_lifetime::CanonicalHost;' not in normalized:
                raise RuntimeError(f'{role}: canonical role tuple/export does not match actual named adapters')
        compiler = subprocess.run([clean[0], '--version'], capture_output=True, check=True).stdout
        receipts.append(dict(role=role, owning_recipe=row, command=command, source_sha256=digest(source),
                             compiler_sha256=digest(clean[0]), compiler_version=compiler.decode(),
                             preprocessed_sha256=hashlib.sha256(preprocess).hexdigest(), log_sha256=digest(log),
                             physical_records=RECORDS))
    stamp.write_text(json.dumps(dict(scope='private socket owner only; no runtime acceptance',
        header_sha256=digest(ROOT / 'src/public/native_socket_host.h'), receipts=receipts), indent=2) + '\n')
    print('Accepted actual socket layouts, signatures and canonical roles:', ', '.join(options.roles))


if __name__ == '__main__':
    main()
