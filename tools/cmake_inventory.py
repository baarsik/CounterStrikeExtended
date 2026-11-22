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
CS_SERVER_BACKENDS = ('default', 'legacy', 'regamedll')


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
    definitions = [path for path in (root / 'cmake').rglob('*')
                   if path.is_file() and (path.suffix == '.cmake' or path.name == 'CMakeLists.txt')]
    definitions.extend((root / 'src/cstrike/regamedll').glob('*.patch'))
    for path in sorted(definitions):
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


def _original_include(path, staged_root, original_root):
    if staged_root:
        include = Path(path).resolve()
        if include.is_relative_to(staged_root):
            return str(original_root / include.relative_to(staged_root))
    return path


@lru_cache(maxsize=16)
def _query(root, configuration, target_os, architecture, diagnostics, cs_server_backend, signature):
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
        if cs_server_backend != 'default':
            command.append('-DCSE_USE_REGAMEDLL=' + ('ON' if cs_server_backend == 'regamedll' else 'OFF'))
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
            staged = properties.pop('CSE_REGAMEDLL_STAGED_SOURCE_ROOT')
            original = properties.pop('CSE_REGAMEDLL_ORIGINAL_SOURCE_ROOT')
            patch = properties.pop('CSE_REGAMEDLL_PATCH')
            revision = properties.pop('CSE_REGAMEDLL_REVISION')
            export_input = properties.pop('CSE_REGAMEDLL_EXPORT_INPUT')
            wrappers = properties.pop('CSE_REGAMEDLL_WRAPPER_ROOT')
            staged_root = Path(staged).resolve() if staged else None
            original_root = Path(original).resolve() if original else None
            wrapper_root = Path(wrappers).resolve() if wrappers else None
            if staged_root and (not original_root or not patch):
                raise ValueError(target + ': staged ReGameDLL inventory requires an original source root and patch')
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
                overlay_path = path.relative_to(staged_root) if staged_root and path.is_relative_to(staged_root) else None
                wrapper_path = path.relative_to(wrapper_root) if wrapper_root and path.is_relative_to(wrapper_root) else None
                source_path = overlay_path or wrapper_path
                original_path = original_root / source_path if source_path else path
                group = groups[source['compileGroupIndex']] if 'compileGroupIndex' in source else {}
                flags = []
                for fragment in group.get('compileCommandFragments', []):
                    # Fragments with a backtrace are declared by the CMake
                    # target/source. Others are host compiler defaults such as
                    # -fPIC/-std and must not be replayed by Windows fixtures.
                    if 'backtrace' in fragment:
                        flags.extend(_split_flags(fragment['fragment']))
                item = {'path': str(original_path), 'language': group.get('language', 'RC' if path.suffix == '.rc' else None),
                        'compile_options': flags,
                        'include_directories': [_original_include(entry['path'], staged_root, original_root)
                                                for entry in group.get('includes', [])],
                        'compile_definitions': [entry['define'] for entry in group.get('defines', [])]}
                if str(path) in resources:
                    resource = resources[str(path)]
                    item['include_directories'] = [_original_include(value, staged_root, original_root)
                                                   for value in resource['INCLUDE_DIRECTORIES'].split(';') if value]
                    item['compile_definitions'] = [value for value in resource['COMPILE_DEFINITIONS'].split(';') if value]
                if overlay_path and not original_path.is_file():
                    item['generated'] = {'overlay_path': overlay_path.as_posix()}
                    if export_input and path.suffix == '.def':
                        item['generated']['source'] = str(Path(export_input).resolve())
                elif wrapper_path:
                    registration = '_'.join(wrapper_path.with_suffix('').parts[1:])
                    item['generated'] = {'source': str(original_path), 'registration': registration}
                elif not overlay_path and path.is_relative_to(build_root.resolve()) and path.is_file():
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
            record = {'target': target, 'runtime': {
                'kind': 'EXECUTABLE' if data['type'] == 'EXECUTABLE' else 'SHARED',
                'name': output, 'directory': relative}, 'sources': sources,
                'link_options': [value for value in properties['LINK_OPTIONS'].split(';') if value],
                'link_libraries': [value for value in properties['LINK_LIBRARIES'].split(';') if value],
                'runtime_resources': [{'source': source, 'destination': destination}
                                      for source, destination in zip(resource_sources, resource_destinations) if source],
                'properties': properties}
            if staged_root:
                record['source_overlay'] = {'root': str(original_root), 'patch': str(Path(patch).resolve()),
                                            'revision': revision}
            targets.append(record)
        return sorted(targets, key=lambda record: record['target'])


def query_targets(root=None, configuration='Release', target_os='windows', architecture='x86', diagnostics=False,
                  *, cs_server_backend='default'):
    """Return isolated records; source membership/settings are evaluated by CMake."""
    _arguments(configuration, target_os, architecture)
    if cs_server_backend not in CS_SERVER_BACKENDS:
        raise ValueError(f'Unsupported Counter-Strike server backend: {cs_server_backend!r}')
    root = Path(root or ROOT).resolve()
    return copy.deepcopy(_query(str(root), configuration, target_os, architecture, bool(diagnostics),
                                cs_server_backend, _definition_signature(root)))


def query_target(root, target, configuration='Release', target_os='windows', architecture='x86', diagnostics=False,
                 *, cs_server_backend='default'):
    for record in query_targets(root, configuration, target_os, architecture, diagnostics,
                                cs_server_backend=cs_server_backend):
        if record['target'] == target:
            return record
    raise ValueError('Unknown CMake runtime target: ' + target)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--configuration', choices=('Debug', 'Release'), default='Release')
    parser.add_argument('--target-os', choices=('windows', 'macos', 'linux'), default='windows')
    parser.add_argument('--architecture', choices=('x86', 'x64', 'arm64'), default='x86')
    parser.add_argument('--diagnostics', action='store_true')
    parser.add_argument('--cs-server-backend', choices=CS_SERVER_BACKENDS, default='default')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    records = query_targets(configuration=args.configuration, target_os=args.target_os,
                            architecture=args.architecture, diagnostics=args.diagnostics,
                            cs_server_backend=args.cs_server_backend)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(records, indent=2) + '\n')
    print('Inspected', len(records), 'CMake runtime targets; metadata only, no cross-compilation.')


if __name__ == '__main__':
    main()
