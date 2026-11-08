#!/usr/bin/env python3
"""Seal a pinned Git source reference without reading or executing working code.

This records source-only evidence. Binary build receipts and authorized runtime
fixtures are separate gates; an archive never constitutes game qualification.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile


def _hash_stream(stream):
    digest = hashlib.sha256()
    for chunk in iter(lambda: stream.read(1024 * 1024), b''):
        digest.update(chunk)
    return digest.hexdigest()


def _hash_file(path):
    with path.open('rb') as stream:
        return _hash_stream(stream)


def capture_source(repository, destination, revision):
    """Create a non-overwriting reference of committed bytes, never working edits."""
    repository = Path(repository).resolve()
    destination = Path(destination).absolute()
    if destination.exists() or destination.is_symlink():
        raise ValueError('Reference destination already exists: ' + str(destination))
    result = subprocess.run(['git', '-C', str(repository), 'rev-parse', '--verify', '--end-of-options',
                             revision + '^{commit}'], capture_output=True, text=True)
    if result.returncode:
        raise ValueError('Invalid reference revision: ' + result.stderr.strip())
    commit = result.stdout.strip()
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.cse-reference-', dir=destination.parent) as temporary:
        staging = Path(temporary) / 'reference'
        staging.mkdir()
        archive = staging / 'source.tar'
        result = subprocess.run(['git', '-C', str(repository), 'archive', '--format=tar',
                                 '--output=' + str(archive), commit], capture_output=True, text=True)
        if result.returncode:
            raise ValueError('Source archive failed: ' + result.stderr.strip())
        files = {}
        with tarfile.open(archive, 'r:') as source:
            for member in source:
                path = Path(member.name)
                if path.is_absolute() or '..' in path.parts:
                    raise ValueError('Unsafe archive path: ' + member.name)
                if member.isfile():
                    with source.extractfile(member) as contents:
                        files[member.name] = dict(size=member.size, sha256=_hash_stream(contents), kind='file')
                elif member.issym():
                    files[member.name] = dict(link=member.linkname, kind='symlink')
                elif not member.isdir():
                    raise ValueError('Unsupported archive entry: ' + member.name)
        report = dict(schema=1, revision=commit, evidence='source-only', qualified=False,
                      source_archive_sha256=_hash_file(archive), files=files)
        manifest = staging / 'manifest.json'
        manifest.write_text(json.dumps(report, indent=2, sort_keys=True) + '\n', encoding='utf-8')
        seal = staging / 'SHA256SUMS'
        seal.write_text(f'{_hash_file(manifest)}  manifest.json\n{_hash_file(archive)}  source.tar\n', encoding='ascii')
        for path in (archive, manifest, seal):
            path.chmod(0o444)
        # mkdir is the non-overwriting reservation. Never replace a concurrently
        # created destination, including an empty directory belonging to a user.
        destination.mkdir()
        try:
            for path in (archive, manifest, seal):
                os.rename(path, destination / path.name)
        except BaseException:
            # A partial reference is explicitly unverifiable; preserve it for
            # diagnosis instead of removing a path potentially used by others.
            raise
    return report


def verify_reference(directory):
    directory = Path(directory)
    seal = (directory / 'SHA256SUMS').read_text(encoding='ascii').splitlines()
    expected_names = {'manifest.json', 'source.tar'}
    seen = set()
    for entry in seal:
        try:
            digest, name = entry.split('  ')
        except ValueError as error:
            raise ValueError('Invalid reference seal') from error
        if name not in expected_names or name in seen or len(digest) != 64:
            raise ValueError('Invalid reference seal entry')
        seen.add(name)
        if _hash_file(directory / name) != digest:
            raise ValueError('Reference hash mismatch: ' + name)
    if seen != expected_names:
        raise ValueError('Incomplete reference seal')
    report = json.loads((directory / 'manifest.json').read_text(encoding='utf-8'))
    if report.get('schema') != 1 or report.get('evidence') != 'source-only' or report.get('qualified') is not False:
        raise ValueError('Invalid source-only reference claims')
    if _hash_file(directory / 'source.tar') != report.get('source_archive_sha256'):
        raise ValueError('Source archive hash differs from manifest')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repository', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--revision')
    parser.add_argument('--verify', action='store_true')
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    if not args.verify and not args.revision:
        parser.error('--revision is required for capture')
    try:
        report = (verify_reference(args.destination) if args.verify else
                  capture_source(args.repository, args.destination, args.revision))
        print(json.dumps({key: value for key, value in report.items() if key != 'files'}, indent=2))
        print('Source files:', len(report['files']))
    except (ValueError, OSError, tarfile.TarError) as error:
        print(str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
