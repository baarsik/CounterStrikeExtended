#!/usr/bin/env python3
"""Query authoritative CMake sources and effective per-source compiler settings.

The query configures production target modules with the host compiler and
symbolic external providers. It inspects metadata, never cross-compiles or
qualifies binaries/providers. Use a real build's File API/compile database for
evidence about an actual toolchain and resolved external dependencies.
"""
import argparse
import copy
from functools import lru_cache
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def _split_flags(fragment):
    if os.name != 'nt':
        return shlex.split(fragment, posix=True)
    # File API fragments use the host generator's command-line quoting. POSIX
    # shlex would remove backslashes from unquoted Windows forced-header paths.
    import ctypes
    from ctypes import wintypes
    parse = ctypes.windll.shell32.CommandLineToArgvW
    parse.argtypes = (wintypes.LPCWSTR, ctypes.POINTER(ctypes.c_int))
    parse.restype = ctypes.POINTER(wintypes.LPWSTR)
    count = ctypes.c_int()
    arguments = parse('cse-query ' + fragment, ctypes.byref(count))
    if not arguments:
        raise ctypes.WinError()
    try:
        return [arguments[index] for index in range(1, count.value)]
    finally:
        release = ctypes.windll.kernel32.LocalFree
        release.argtypes = (ctypes.c_void_p,)
        release.restype = ctypes.c_void_p
        release(arguments)


def _definition_signature(root):
    digest = hashlib.sha256()
    for path in sorted((root / 'cmake').rglob('*')):
        if path.is_file() and (path.suffix == '.cmake' or path.name == 'CMakeLists.txt'):
            digest.update(str(path.relative_to(root)).encode())
            digest.update(path.read_bytes())
    return digest.hexdigest()


def _arguments(configuration, target_os, architecture):
    if configuration not in ('Debug', 'Release'):
        raise ValueError('Configuration must be Debug or Release')
    if target_os not in ('windows', 'macos', 'linux'):
        raise ValueError('Unsupported target OS: ' + target_os)
    if architecture not in ('x86', 'x64', 'arm64'):
        raise ValueError('Unsupported architecture: ' + architecture)


def _absolute(path, source_root, build_root, generated=False):
    path = Path(path)
    if path.is_absolute():
        return path.resolve()
    return ((build_root if generated else source_root) / path).resolve()


@lru_cache(maxsize=16)
def _query(root, configuration, target_os, architecture, diagnostics, signature):
    root = Path(root)
    with tempfile.TemporaryDirectory(prefix='cse-cmake-inventory-') as temporary:
        build = Path(temporary)
        query = build / '.cmake/api/v1/query'
        query.mkdir(parents=True)
        (query / 'codemodel-v2').touch()
        command = ['cmake', '-S', str(root / 'cmake/query'), '-B', str(build), '-G', 'Ninja',
                   '-DCMAKE_BUILD_TYPE=' + configuration, '-DCSE_QUERY_OS=' + target_os,
                   '-DCSE_QUERY_ARCH=' + architecture,
                   '-DCSE_RELEASE_DIAGNOSTICS=' + ('ON' if diagnostics else 'OFF')]
        result = subprocess.run(command, capture_output=True, text=True)
        if result.returncode:
            raise ValueError('CMake inventory configuration failed:\n' + result.stdout + result.stderr)
        reply = build / '.cmake/api/v1/reply'
        index = json.loads(next(reply.glob('index-*.json')).read_text())
        model = json.loads((reply / index['reply']['codemodel-v2']['jsonFile']).read_text())
        source_root = Path(model['paths']['source'])
        build_root = Path(model['paths']['build'])
        targets = []
        for reference in model['configurations'][0]['targets']:
            target = reference['name']
            property_file = build / 'properties' / (target + '-' + configuration + '.json')
            if not property_file.is_file():
                continue
            properties = json.loads(property_file.read_text())
            resource_file = property_file.with_name(target + '-' + configuration + '-resources.json')
            resources = {str(Path(item['path']).resolve()): item for item in json.loads(resource_file.read_text())}
            data = json.loads((reply / reference['jsonFile']).read_text())
            groups = data.get('compileGroups', [])
            sources = []
            for source in data.get('sources', []):
                path = _absolute(source['path'], source_root, build_root, source.get('isGenerated', False))
                # CMake may include rule/dependency artifacts alongside sources.
                if path.suffix in ('.rule', '.stamp'):
                    continue
                group = groups[source['compileGroupIndex']] if 'compileGroupIndex' in source else {}
                flags = []
                for fragment in group.get('compileCommandFragments', []):
                    # Fragments with a backtrace are declared by the CMake
                    # target/source. Others are host compiler defaults such as
                    # -fPIC/-std and must not be replayed by Windows fixtures.
                    if 'backtrace' in fragment:
                        flags.extend(_split_flags(fragment['fragment']))
                item = {'path': str(path), 'language': group.get('language', 'RC' if path.suffix == '.rc' else None),
                        'compile_options': flags,
                        'include_directories': [entry['path'] for entry in group.get('includes', [])],
                        'compile_definitions': [entry['define'] for entry in group.get('defines', [])]}
                if str(path) in resources:
                    resource = resources[str(path)]
                    item['include_directories'] = [value for value in resource['INCLUDE_DIRECTORIES'].split(';') if value]
                    item['compile_definitions'] = [value for value in resource['COMPILE_DEFINITIONS'].split(';') if value]
                if path.is_relative_to(build_root.resolve()) and path.is_file():
                    text = path.read_text(errors='replace')
                    match = re.fullmatch(r'#define (A3D_GEOMETRY_\w+)\n#include "([^"]+)"\n', text)
                    if match:
                        item['generated'] = {'section': match[1], 'source': match[2]}
                sources.append(item)
            output = properties['OUTPUT_NAME'] or target
            directory = properties['RUNTIME_OUTPUT_DIRECTORY']
            runtime_root = (build / 'runtime' / configuration).resolve()
            relative = Path(directory).resolve().relative_to(runtime_root).as_posix()
            if relative == '.':
                relative = ''
            resource_sources = properties['CSE_QUERY_RESOURCE_SOURCES'].split(';')
            resource_destinations = properties['CSE_QUERY_RESOURCE_DESTINATIONS'].split(';')
            targets.append({'target': target, 'runtime': {
                'kind': 'EXECUTABLE' if data['type'] == 'EXECUTABLE' else 'SHARED',
                'name': output, 'directory': relative}, 'sources': sources,
                'link_options': [value for value in properties['LINK_OPTIONS'].split(';') if value],
                'link_libraries': [value for value in properties['LINK_LIBRARIES'].split(';') if value],
                'runtime_resources': [{'source': source, 'destination': destination}
                                      for source, destination in zip(resource_sources, resource_destinations) if source],
                'properties': properties})
        return sorted(targets, key=lambda record: record['target'])


def query_targets(root=None, configuration='Release', target_os='windows', architecture='x86', diagnostics=False):
    """Return isolated records; source membership/settings are evaluated by CMake."""
    _arguments(configuration, target_os, architecture)
    root = Path(root or ROOT).resolve()
    return copy.deepcopy(_query(str(root), configuration, target_os, architecture, bool(diagnostics),
                                _definition_signature(root)))


def query_target(root, target, configuration='Release', target_os='windows', architecture='x86', diagnostics=False):
    for record in query_targets(root, configuration, target_os, architecture, diagnostics):
        if record['target'] == target:
            return record
    raise ValueError('Unknown CMake runtime target: ' + target)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--configuration', choices=('Debug', 'Release'), default='Release')
    parser.add_argument('--target-os', choices=('windows', 'macos', 'linux'), default='windows')
    parser.add_argument('--architecture', choices=('x86', 'x64', 'arm64'), default='x86')
    parser.add_argument('--diagnostics', action='store_true')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    records = query_targets(configuration=args.configuration, target_os=args.target_os,
                            architecture=args.architecture, diagnostics=args.diagnostics)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(records, indent=2) + '\n')
    print('Inspected', len(records), 'CMake runtime targets; metadata only, no cross-compilation.')


if __name__ == '__main__':
    main()
