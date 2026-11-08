#!/usr/bin/env python3
"""Inspect complete Windows production outputs using the generated inventory.

Reports architecture, hashes, exports/ordinals, imports and resource bytes.
Does not load modules, resolve runtime-only providers, or qualify gameplay.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath, PureWindowsPath
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from binary_identity import looks_native, require_architecture
from audit_sdl3 import is_system_dll, pe_info

ROOT = Path(__file__).resolve().parents[2]
NON_RUNTIME_EXTENSIONS = {'.lib', '.a', '.exp', '.obj', '.o', '.pdb', '.ilk', '.map', '.cmake', '.tlog'}


def _expected_outputs(records):
    outputs = {}
    paths = set()
    for record in records:
        runtime = record['runtime']
        kind = runtime['kind']
        name = runtime['name']
        directory = runtime['directory'].replace('\\', '/')
        if kind not in ('EXECUTABLE', 'SHARED') or not name or '/' in name or '\\' in name or ':' in name:
            raise ValueError('Invalid runtime output name/kind')
        relative = PurePosixPath(directory) / (name + ('.exe' if kind == 'EXECUTABLE' else '.dll'))
        if relative.is_absolute() or PureWindowsPath(str(relative)).drive or '..' in relative.parts:
            raise ValueError('Invalid runtime output path: ' + str(relative))
        target = record['target']
        if target in outputs or str(relative).casefold() in paths:
            raise ValueError('Duplicate production target/output: ' + target)
        outputs[target] = str(relative)
        paths.add(str(relative).casefold())
    if not outputs:
        raise ValueError('Empty production inventory')
    return outputs


def audit_inventory(root, records, architecture):
    root = Path(root).resolve()
    if not root.is_dir():
        raise ValueError('Runtime directory is missing: ' + str(root))
    outputs = _expected_outputs(records)
    for target, relative in outputs.items():
        path = root / relative
        if not path.is_file():
            raise ValueError(f'{target}: missing production output {relative}')
        if not path.resolve().is_relative_to(root):
            raise ValueError('Runtime output escapes root: ' + relative)
    binaries = {}
    for path in sorted(root.rglob('*')):
        if not path.is_file():
            continue
        if not path.resolve().is_relative_to(root):
            raise ValueError('Runtime file escapes root: ' + str(path))
        relative = path.relative_to(root).as_posix()
        if path.suffix.lower() in NON_RUNTIME_EXTENSIONS:
            raise ValueError('Non-runtime product in output: ' + relative)
        with path.open('rb') as stream:
            native_content = looks_native(stream.read(8))
        if not native_content and path.suffix.lower() not in ('.exe', '.dll'):
            continue
        data = path.read_bytes()
        identity = require_architecture(data, architecture)
        if identity['format'] != 'pe':
            raise ValueError('Windows runtime output is not PE: ' + relative)
        if relative.casefold() in {name.casefold() for name in binaries}:
            raise ValueError('Duplicate case-insensitive runtime path: ' + relative)
        binaries[relative] = dict(pe_info(data), architectures=identity['architectures'],
                                  sha256=hashlib.sha256(data).hexdigest(), size=len(data))
    available = {name.casefold() for name in binaries}
    unresolved = []
    for relative, info in binaries.items():
        for dependency in info['imports']:
            if is_system_dll(dependency):
                continue
            candidates = {dependency.casefold(), str(PurePosixPath(relative).parent / dependency).casefold()}
            if not candidates.intersection(available):
                unresolved.append(dict(consumer=relative, dependency=dependency))
    return dict(schema=1, evidence='static-binary-inspection', qualified=False, architecture=architecture,
                production_targets=outputs, binaries=binaries, unresolved_imports=unresolved,
                runtime_only_provider_closure='not-tested')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('runtime', type=Path)
    parser.add_argument('--inventory', type=Path, default=ROOT / 'docs/msbuild/repository-selections.json')
    parser.add_argument('--architecture', choices=['x86', 'x64', 'arm64'], required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        records = json.loads(args.inventory.read_text(encoding='utf-8'))
        report = audit_inventory(args.runtime, records, args.architecture)
        if args.output.exists():
            raise ValueError('Evidence output already exists: ' + str(args.output))
        with args.output.open('x', encoding='utf-8') as output:
            json.dump(report, output, indent=2)
            output.write('\n')
        print(f"Inspected {len(report['production_targets'])} production targets, {len(report['binaries'])} binaries; "
              f"{len(report['unresolved_imports'])} unresolved formal imports. Runtime qualification: not tested.")
    except (ValueError, OSError, KeyError) as error:
        print(str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
