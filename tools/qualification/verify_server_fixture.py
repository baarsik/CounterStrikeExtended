#!/usr/bin/env python3
"""Verify pinned, unchanged external Win32 server packages without executing them.

Acquisition is explicit: put the official archives named in the lock in a cache
and run this verifier. This does not build/patch ReHLDS, Metamod or AMX Mod X.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath, PureWindowsPath
import re
import sys
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from binary_identity import require_architecture

DEFAULT_LOCK = Path(__file__).parent / 'fixtures/win32-server/lock.json'


def _relative(name):
    if not isinstance(name, str) or not name or '\\' in name:
        raise ValueError('Invalid package path')
    path = PurePosixPath(name)
    if path.is_absolute() or '..' in path.parts or PureWindowsPath(name).drive:
        raise ValueError('Unsafe package path: ' + name)
    return path


def _check_hash(data, expected, label):
    if not isinstance(expected, str) or not re.fullmatch('[0-9a-f]{64}', expected):
        raise ValueError('Invalid pinned hash: ' + label)
    if hashlib.sha256(data).hexdigest() != expected:
        raise ValueError('Pinned hash mismatch: ' + label)


def verify_packages(lock, directory):
    if lock.get('schema') != 1 or lock.get('architecture') != 'x86' or not lock.get('packages'):
        raise ValueError('Invalid Win32 server package lock')
    reports = {}
    for package in lock['packages']:
        name = package['archive']
        if len(_relative(name).parts) != 1 or name in reports:
            raise ValueError('Invalid or duplicate archive name')
        path = Path(directory) / name
        _check_hash(path.read_bytes(), package['sha256'], name)
        native_files = package['native_files']
        if not native_files:
            raise ValueError('Package has no pinned native files: ' + name)
        with zipfile.ZipFile(path) as archive:
            members = archive.namelist()
            if len(members) != len(set(members)):
                raise ValueError('Duplicate archive member: ' + name)
            for member in members:
                _relative(member)
            observed = {member for member in members if member.lower().endswith(('.dll', '.exe'))}
            if observed != set(native_files):
                raise ValueError('Native package inventory differs from lock: ' + name)
            for member, digest in native_files.items():
                _relative(member)
                data = archive.read(member)
                _check_hash(data, digest, member)
                if require_architecture(data, 'x86')['format'] != 'pe':
                    raise ValueError('Win32 package member is not PE: ' + member)
        reports[name] = dict(version=package['version'], sha256=package['sha256'], native_files=sorted(native_files))
    return dict(schema=1, evidence='pinned-external-server-binaries', qualified=False, packages=reports)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--lock', type=Path, default=DEFAULT_LOCK)
    parser.add_argument('--archive-directory', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        report = verify_packages(json.loads(args.lock.read_text(encoding='utf-8')), args.archive_directory)
        with args.output.open('x', encoding='utf-8') as output:
            json.dump(report, output, indent=2)
            output.write('\n')
        print(f"Verified {len(report['packages'])} official Win32 server archives. Runtime qualification: not run.")
    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
        print(str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
