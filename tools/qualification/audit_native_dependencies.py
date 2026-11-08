#!/usr/bin/env python3
"""Compare source-built dependency recipes with the retained Win32 provenance.

This is static provenance/machine evidence. It does not execute code, establish
source-to-binary reproducibility by itself, or qualify game/provider behavior.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from binary_identity import looks_native, require_architecture

ROOT = Path(__file__).resolve().parents[2]
REFERENCE = ROOT / 'GoldSrc-SDK/external/installed/x86-windows-static-md/share'
MANIFEST = ROOT / 'cmake/dependencies/vcpkg/vcpkg.json'


def _source_identity(document):
    packages = {item['SPDXID']: item for item in document['packages']}
    port = packages['SPDXRef-port']
    resources = sorted((dict(location=item['downloadLocation'], checksums=item['checksums'])
                        for key, item in packages.items() if key.startswith('SPDXRef-resource-')),
                       key=lambda item: item['location'])
    files = {item['fileName']: item['checksums'] for item in document['files']
             if item['SPDXID'].startswith('SPDXRef-port-file-')}
    if not resources or not files:
        raise ValueError('Missing source/recipe provenance')
    identity = dict(version=port['versionInfo'], name=port['name'],
                    files=files, resources=resources)
    return identity


def audit_dependencies(prefix, reference, packages, architecture, *, maximum_macos=None):
    prefix = Path(prefix).resolve(strict=True)
    reference = Path(reference)
    reports = {}
    for name in packages:
        if not name or Path(name).name != name or name in ('.', '..'):
            raise ValueError('Invalid dependency name')
        expected = _source_identity(json.loads((reference / name / 'vcpkg.spdx.json').read_text()))
        observed = _source_identity(json.loads((prefix / 'share' / name / 'vcpkg.spdx.json').read_text()))
        if observed != expected:
            raise ValueError('Dependency source/recipe differs from Win32 reference: ' + name)
        reports[name] = dict(version=observed['version'], recipe_files=len(observed['files']),
                             source_archives=len(observed['resources']))
    binaries = {}
    for label, relative in [('Release', 'lib'), ('Debug', 'debug/lib')]:
        count = 0
        for path in sorted((prefix / relative).rglob('*')):
            if not path.is_file():
                continue
            if not path.resolve().is_relative_to(prefix):
                raise ValueError('Dependency binary escapes install prefix: ' + str(path))
            data = path.read_bytes()
            if not looks_native(data) and path.suffix.lower() not in ('.lib', '.a', '.dll', '.dylib', '.so'):
                continue
            identity = require_architecture(data, architecture, maximum_macos=maximum_macos)
            binaries[path.relative_to(prefix).as_posix()] = dict(
                sha256=hashlib.sha256(data).hexdigest(), identity=identity)
            count += 1
        if not count:
            raise ValueError('Missing ' + label + ' native libraries')
    return dict(schema=1, evidence='dependency-source-provenance-and-static-architecture', qualified=False,
                architecture=architecture, packages=reports, binaries=binaries)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prefix', type=Path, required=True)
    parser.add_argument('--architecture', choices=['x86', 'x64', 'arm64'], required=True)
    parser.add_argument('--platform', choices=['windows', 'macos', 'linux'], required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        manifest = json.loads(MANIFEST.read_text())
        packages = []
        for dependency in manifest['dependencies']:
            platform = dependency.get('platform')
            if platform and not ((platform == 'windows' and args.platform == 'windows') or
                                 (platform == 'osx | linux' and args.platform in ('macos', 'linux'))):
                continue
            packages.append(dependency['name'])
        report = audit_dependencies(args.prefix, REFERENCE, packages, args.architecture,
                                    maximum_macos=(14, 0, 0) if args.platform == 'macos' else None)
        report['registry_baseline'] = manifest['builtin-baseline']
        with args.output.open('x') as output:
            json.dump(report, output, indent=2)
            output.write('\n')
        print(f"Verified {len(packages)} dependency recipes and {len(report['binaries'])} native archives; runtime unqualified.")
    except (OSError, ValueError, KeyError) as error:
        print(str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
