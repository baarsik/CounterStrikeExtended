#!/usr/bin/env python3
"""Translate this fork's retained repository manifests; never invokes MSBuild.

Microsoft toolchain/user defaults are outside this source-only evaluator. The
original manifests remain archived, and actual Windows evaluation is separate.
"""
from pathlib import Path
import collections
import json
import hashlib
import os
import re
import xml.etree.ElementTree as ET

from game_module_layouts import generate as generate_game_module_layouts
from client_module_layouts import generate as generate_client_module_layouts
from game_module_exports import native_definition
from portable_game_policy import settings as portable_game_settings
from sdl3_dependency import validate_payload
from saved_callback_catalog import generate as generate_callback_catalog

ROOT = Path(__file__).resolve().parents[1]
SDK = ROOT / 'GoldSrc-SDK'
NS = '{http://schemas.microsoft.com/developer/msbuild/2003}'
TARGETS = {'launcher': 'launcher', 'hw_engine': 'hw_engine', 'hlds': 'hlds',
           'swds': 'swds', 'cstrike_cdll': 'cs_client', 'cstrikedll': 'cs_server',
           'hl_cdll': 'hl_client', 'hldll': 'hl_server',
           'filesystem_stdio': 'filesystem_stdio', 'tier0': 'tier0', 'vgui': 'vgui',
           'vgui2': 'vgui2', 'GameUI': 'gameui', 'particleman': 'particleman',
           'Core': 'hltv_core', 'demoplayer': 'demoplayer',
           'HLTV': 'hltv', 'Proxy': 'hltv_proxy', 'a3dapi': 'a3dapi'}
CASE_PATHS = {str(p).lower(): p for p in SDK.rglob('*')}
SYSTEM_LIBS = set('kernel32 user32 gdi32 advapi32 ws2_32 wsock32 winmm shell32 shlwapi opengl32 ddraw dxguid dsound version ole32 uuid'.split())

# Exact shared-build contract: changing props/targets requires an intentional
# translator update. Unknown actions must never disappear during translation.
SDL3_BUILD_HASHES = {
    'props': 'a0ab9bb3d4fdee1e725e718b84e945291c340081063aefa921ca9b86af5f5ea9',
    'targets': 'e5e2833656cf8fdef8ee68d2369fc6e9810247b18715008379ece020bab874d6',
}


MPG123_BUILD_HASHES = {'props': 'd2f000bae7304c2da48202ec8ff6d964de89a93d293254dcfa218fd9e82c0c8c', 'targets': '9ead8a1fa2f92e5e78dcefbd357251ca7bbcbea4d3c0ae29c6ce779ef97b4621'}


def validate_mpg123_build_file(path, kind):
    if hashlib.sha256(path.read_bytes()).hexdigest() != MPG123_BUILD_HASHES[kind]:
        raise ValueError("Unsupported mpg123 build contract: " + str(path))


def validate_sdl3_build_file(path, kind):
    if hashlib.sha256(path.read_bytes()).hexdigest() != SDL3_BUILD_HASHES[kind]:
        raise ValueError('Unsupported SDL3 build contract: ' + str(path))


def import_command(target, config, path, data):
    if str(path) == data.get('mpg123', {}).get('import_library'):
        return f'cse_link_mpg123({target} {config})'
    if path.name.lower() == 'mpg123.lib':
        raise ValueError('Unpinned mpg123 import: ' + str(path))
    pinned = SDK / 'external/SDL3/lib/x86/SDL3.lib'
    if path == pinned:
        if not data.get('sdl3', {}).get('shared'):
            raise ValueError('SDL3 import requires the shared build contract')
        return f'cse_link_sdl3({target} {config})'
    if path.name.lower() == 'sdl3.lib':
        raise ValueError('Unpinned SDL3 import: ' + str(path))
    if path == SDK / 'lib/public/steam_api.lib':
        return f'cse_link_steam({target} {config} {quote(portable(str(path)))})'
    if path.name.lower() == 'steam_api.lib':
        raise ValueError('Unpinned Steam import: ' + str(path))
    return f'cse_link_import({target} {config} {quote(portable(str(path)))})'


def name(e):
    return e.tag.removeprefix(NS)


def split(value):
    return [v.strip() for v in value.split(';') if v.strip()]


def source_path(value, parent):
    value = value.replace('\\', '/')
    if '$(' in value or '%(' in value:
        raise ValueError('Unresolved path: ' + value)
    p = Path(os.path.normpath(str(parent / value)))
    return CASE_PATHS.get(str(p).lower(), p)


def portable(value):
    return value.replace(str(SDK), '${CSE_SDK_DIR}')


def evaluate(project, configuration):
    properties = {'ProjectDir': str(project.parent) + '/', 'MSBuildProjectDirectory': str(project.parent), 'ProjectName': project.stem,
                  'MSBuildProjectName': project.stem, 'Configuration': configuration,
                  'Platform': 'Win32', 'RunCodeAnalysis': 'false', 'MaxSdkDir': '',
                  'CharacterStudioSdkDir': ''}
    definitions = {}
    raw_items = []
    imports = []
    generating_targets = []
    sdl3_imports = set()
    mpg123_imports = set()

    def expand(value):
        for _ in range(20):
            result = re.sub(r'\$\(([A-Za-z_][A-Za-z_0-9]*)\)',
                            lambda m: properties.get(m[1], m[0]), value)
            if result == value:
                break
            value = result
        def normalized(m):
            parts = re.findall(r"'([^']*)'", m[1])
            return os.path.normpath('/'.join(parts).replace('\\', '/')) + '/'
        return re.sub(r'\$\(\[MSBuild\]::NormalizeDirectory\((.*?)\)\)', normalized, value)

    def condition(value):
        if not value:
            return True
        value = expand(value).strip()
        if value.lower().startswith('exists('):
            # User-specific imports are intentionally not used by this translation.
            if 'UserRootDir' not in value:
                raise ValueError('Unexpected Exists condition: ' + value)
            return False
        parts = re.split(r'\s+or\s+', value, flags=re.I)
        if len(parts) > 1:
            return any(condition(p) for p in parts)
        parts = re.split(r'\s+and\s+', value, flags=re.I)
        if len(parts) > 1:
            return all(condition(p) for p in parts)
        m = re.fullmatch(r"'([^']*)'\s*(==|!=)\s*'([^']*)'", value)
        if not m:
            raise ValueError('Unsupported condition: ' + value)
        same = m[1].lower() == m[3].lower()
        return same if m[2] == '==' else not same

    def metadata(nodes, inherited):
        result = dict(inherited)
        for node in nodes:
            if not condition(node.get('Condition')):
                continue
            key = name(node)
            value = expand((node.text or '').strip())
            if '$([MSBuild]::Unescape' in value:
                # The only .NET expression in retained manifests removes NDEBUG.
                if key != 'PreprocessorDefinitions' or not value.endswith(';UTL_ENGINE_ASSERTS'):
                    raise ValueError('Unexpected property function: ' + value)
                value = ';'.join(d for d in split(result.get(key, '')) if d != 'NDEBUG') + ';UTL_ENGINE_ASSERTS'
            else:
                value = re.sub(r'%\(([A-Za-z0-9_]+)\)', lambda m: result.get(m[1], ''), value)
            result[key] = value
        return result

    def walk(tree, current):
        previous = properties.get('MSBuildThisFileDirectory')
        properties['MSBuildThisFileDirectory'] = str(current.parent) + '/'
        try:
            walk_elements(tree, current)
        finally:
            if previous is None:
                properties.pop('MSBuildThisFileDirectory', None)
            else:
                properties['MSBuildThisFileDirectory'] = previous

    def walk_elements(tree, current):
        for element in tree:
            if not condition(element.get('Condition')):
                continue
            kind = name(element)
            if kind == 'Import':
                value = expand(element.attrib['Project'])
                if 'VCTargetsPath' in value or 'UserRootDir' in value:
                    imports.append({'path': value, 'evaluated': False})
                else:
                    path = source_path(value, current.parent)
                    imports.append({'path': str(path.relative_to(SDK)), 'evaluated': True})
                    if path in [SDK / 'projects/vs2026/SDL3.props', SDK / 'projects/vs2026/SDL3.targets']:
                        kind = path.suffix[1:]
                        validate_sdl3_build_file(path, kind)
                        sdl3_imports.add(kind)
                    if path in [SDK / 'projects/vs2026/Mpg123.props', SDK / 'projects/vs2026/Mpg123.targets']:
                        kind = path.suffix[1:]
                        validate_mpg123_build_file(path, kind)
                        mpg123_imports.add(kind)
                    walk(ET.parse(path).getroot(), path)
            elif kind == 'ImportGroup':
                walk(element, current)
            elif kind == 'PropertyGroup':
                for node in element:
                    if condition(node.get('Condition')):
                        properties[name(node)] = expand((node.text or '').strip())
            elif kind == 'ItemDefinitionGroup':
                for node in element:
                    if condition(node.get('Condition')):
                        definitions[name(node)] = metadata(node, definitions.get(name(node), {}))
            elif kind == 'Target':
                if current == SDK / 'projects/vs2026/Mpg123.targets' and 'targets' in mpg123_imports:
                    continue
                if current == SDK / 'projects/vs2026/SDL3.targets' and 'targets' in sdl3_imports:
                    # Whole-file validation above covers every task and attribute.
                    continue
                if project.stem != 'a3dapi' or element.get('Name') != 'PrepareGeometryCompilation':
                    raise ValueError('Unsupported project target: ' + str(current) + ' / ' + str(element.get('Name')))
                generating_targets.append(element)
            elif kind == 'UsingTask':
                raise ValueError('Unsupported custom task: ' + str(current))
            elif kind == 'ItemGroup':
                for node in element:
                    if condition(node.get('Condition')) and 'Include' in node.attrib:
                        raw_items.append((name(node), expand(node.attrib['Include']), list(node)))
    walk(ET.parse(project).getroot(), project)
    items = []
    for kind, value, nodes in raw_items:
        for entry in split(value):
            if kind == 'ProjectConfiguration':
                continue
            items.append({'kind': kind, 'path': str(source_path(entry, project.parent)),
                          'metadata': metadata(nodes, definitions.get(kind, {})),
                          'overrides': metadata(nodes, {})})
    for target in generating_targets:
        for action in target:
            if name(action) not in ['MakeDir', 'ItemGroup', 'WriteLinesToFile']:
                raise ValueError('Unsupported source-generation action: ' + name(action))
        writer = target.find(NS + 'WriteLinesToFile')
        expected_lines = '#define %(_GeometryCompile.Section)\n#include "$(A3dSourceDir)A3dGeom.cpp"'
        if writer is None or writer.get('Lines') != expected_lines:
            raise ValueError('Unexpected A3D source-generation template')
        for geometry in target.findall('./' + NS + 'ItemGroup/' + NS + '_GeometryCompile'):
            overrides = metadata(geometry, {})
            effective = dict(definitions['ClCompile'])
            effective.update(overrides)
            items.append({'kind': 'ClCompile',
                          'path': str(source_path(expand(geometry.attrib['Include']), project.parent)),
                          'metadata': effective, 'overrides': overrides,
                          'generated': {'section': overrides['Section'],
                                        'include': str(SDK / 'a3dapi/A3dGeom.cpp')}})
    result = {'properties': properties, 'definitions': definitions, 'items': items, 'imports': imports}
    if sdl3_imports:
        if sdl3_imports != {'props', 'targets'}:
            raise ValueError('SDL3 requires both shared props and targets imports')
        validate_payload(SDK / 'external/SDL3')
        expected = {
            'CseSdl3Root': SDK / 'external/SDL3',
            'CseSdl3Include': SDK / 'external/SDL3/include',
            'CseSdl3ImportLibrary': SDK / 'external/SDL3/lib/x86/SDL3.lib',
            'CseSdl3RuntimeDll': SDK / 'external/SDL3/lib/x86/SDL3.dll',
            'CseSdl3Manifest': SDK / 'external/SDL3/SHA256SUMS',
            'CseSdl3RuntimeRoot': SDK / 'bin/Win32' / configuration,
        }
        for key, path in expected.items():
            if source_path(properties.get(key, ''), project.parent) != path:
                raise ValueError('Unexpected SDL3 property ' + key)
        if properties.get('CseSdl3Version') != '3.4.20' or properties.get('CseSdl3Shared') not in ['true', 'false']:
            raise ValueError('Unexpected SDL3 version/shared contract')
        shared = properties['CseSdl3Shared'] == 'true'
        dependencies = split(definitions.get('Link', {}).get('AdditionalDependencies', ''))
        has_import = any(source_path(lib, project.parent) == expected['CseSdl3ImportLibrary']
                         for lib in dependencies)
        if shared != has_import:
            raise ValueError('SDL3 shared build contract must retain its exact import library')
        result['sdl3'] = {'version': '3.4.20', 'shared': shared,
                          'include_root': str(expected['CseSdl3Include']),
                          'import_library': str(expected['CseSdl3ImportLibrary']),
                          'runtime_dll': str(expected['CseSdl3RuntimeDll']),
                          'runtime_root': str(expected['CseSdl3RuntimeRoot']),
                          'validation_manifest': str(expected['CseSdl3Manifest']),
                          'validation_target': 'CseValidateSdl3',
                          'staging_target': 'CseStageSdl3' if shared else None}
    if mpg123_imports:
        if mpg123_imports != {'props', 'targets'}:
            raise ValueError('mpg123 requires both shared props and targets imports')
        build_root = source_path(properties['IntDir'], project.parent) / 'mpg123'
        expected = {
            'CseMpg123Archive': SDK / 'external/mpg123/mpg123-1.33.7.tar.bz2',
            'CseMpg123BuildRoot': build_root,
            'CseMpg123Source': build_root / 'source/mpg123-1.33.7',
            'CseMpg123Include': build_root / 'source/mpg123-1.33.7/src/include',
            'CseMpg123ImportLibrary': build_root / 'lib' / configuration / 'mpg123/mpg123.lib',
            'CseMpg123RuntimeDll': build_root / 'runtime' / configuration / 'mpg123.dll',
            'CseMpg123RuntimeRoot': SDK / 'bin/Win32' / configuration,
            'CseMpg123CmakeSource': ROOT / 'cmake/dependencies/mpg123',
        }
        if properties.get('CseMpg123Version') != '1.33.7':
            raise ValueError('Unexpected mpg123 version')
        for key, path in expected.items():
            if source_path(properties.get(key, ''), project.parent) != path:
                raise ValueError('Unexpected mpg123 property ' + key)
        archive = expected['CseMpg123Archive']
        source_hash = '31d0e35a4ca567ec9b5ebda6c3062bb4435d6d3eacd6ef0d95cadd7854dc03ee'
        if hashlib.sha256(archive.read_bytes()).hexdigest() != source_hash:
            raise ValueError('mpg123 source archive hash mismatch')
        dependencies = split(definitions.get('Link', {}).get('AdditionalDependencies', ''))
        if not any(source_path(lib, project.parent) == expected['CseMpg123ImportLibrary'] for lib in dependencies):
            raise ValueError('mpg123 build contract must retain its generated import library')
        result['mpg123'] = {
            'version': '1.33.7', 'archive': str(archive), 'source_sha256': source_hash,
            'include_root': str(expected['CseMpg123Include']),
            'import_library': str(expected['CseMpg123ImportLibrary']),
            'runtime_dll': str(expected['CseMpg123RuntimeDll']),
            'build_target': 'CseBuildMpg123', 'staging_target': 'CseStageMpg123',
        }
    return result


COMPILE = {
    'WarningLevel': {'Level3': '/W3', 'Level4': '/W4'},
    'TreatWarningAsError': {'true': '/WX', 'false': '/WX-'},
    'Optimization': {'Disabled': '/Od', 'MaxSpeed': '/O2', 'MinSpace': '/O1'},
    'ExceptionHandling': {'Sync': '/EHsc', 'Async': '/EHa', 'false': '/EHs-c-'},
    'RuntimeTypeInfo': {'true': '/GR', 'false': '/GR-'},
    'FloatingPointModel': {'Precise': '/fp:precise', 'Strict': '/fp:strict', 'Fast': '/fp:fast'},
    'EnableEnhancedInstructionSet': {'StreamingSIMDExtensions2': '/arch:SSE2', 'NoExtensions': '/arch:IA32'},
    'BasicRuntimeChecks': {'EnableFastChecks': '/RTC1', 'Default': ''},
    'FunctionLevelLinking': {'true': '/Gy', 'false': '/Gy-'},
    'IntrinsicFunctions': {'true': '/Oi', 'false': '/Oi-'},
    'BufferSecurityCheck': {'true': '/GS', 'false': '/GS-'},
    'SDLCheck': {'true': '/sdl', 'false': '/sdl-'},
    'ConformanceMode': {'true': '/permissive-', 'false': '/permissive'},
    'DebugInformationFormat': {'ProgramDatabase': '/Zi'},
    'InlineFunctionExpansion': {'OnlyExplicitInline': '/Ob1', 'Disabled': '/Ob0'},
    'CallingConvention': {'Cdecl': '/Gd', 'StdCall': '/Gz', 'FastCall': '/Gr'},
}
LINK = {'EnableCOMDATFolding': {'true': '/OPT:ICF', 'false': '/OPT:NOICF'},
        'OptimizeReferences': {'true': '/OPT:REF', 'false': '/OPT:NOREF'},
        'GenerateDebugInformation': {'true': '/DEBUG', 'false': '/DEBUG:NONE'},
        'TreatLinkerWarningAsErrors': {'true': '/WX', 'false': '/WX:NO'},
        'LargeAddressAware': {'true': '/LARGEADDRESSAWARE', 'false': '/LARGEADDRESSAWARE:NO'},
        'RandomizedBaseAddress': {'true': '/DYNAMICBASE', 'false': '/DYNAMICBASE:NO'},
        'DataExecutionPrevention': {'true': '/NXCOMPAT', 'false': '/NXCOMPAT:NO'},
        'SetChecksum': {'true': '/RELEASE'},
        'LinkTimeCodeGeneration': {'UseLinkTimeCodeGeneration': '/LTCG'},
        'TargetMachine': {'MachineX86': '/MACHINE:${CSE_MSVC_MACHINE}'},
        'SubSystem': {'Windows': '/SUBSYSTEM:WINDOWS', 'Console': '/SUBSYSTEM:CONSOLE'}}


def options(metadata, table):
    result = []
    for key, variants in table.items():
        if key in metadata and metadata[key]:
            result.append(variants[metadata[key]])
    if table is COMPILE:
        if metadata.get('StructMemberAlignment'):
            result.append('/Zp' + metadata['StructMemberAlignment'].removeprefix('byte'))
        result.extend('/wd' + v for v in split(metadata.get('DisableSpecificWarnings', '')))
    return [v for v in result if v]


def additional_options(value):
    # Retained additional options contain /external:I paths or plain linker options.
    tokens = re.findall(r'(?:[^\s"]|"[^"]*")+', value)
    out = []
    for token in tokens:
        token = token.replace('"', '')
        if token.startswith('/external:I'):
            token = '/external:I' + str(source_path(token[len('/external:I'):], SDK))
        out.append(portable(token))
    return out


def quote(value):
    return '"' + value.replace('\\', '/').replace('"', '\\"') + '"'


def option_predicate(configuration, value):
    predicate = f'$<CONFIG:{configuration}>'
    # Retained preferred image bases belong to the original Win32 image layout.
    # Native PE images use the linker's architecture-appropriate ASLR base.
    if value in ('/arch:SSE2', '/arch:IA32') or value.upper().startswith('/BASE:'):
        predicate = f'$<AND:{predicate},$<STREQUAL:${{CSE_TARGET_ARCH}},x86>>'
    diagnostic = (value in ['/Zi', '/ZI', '/Z7'] or
                  value.upper().startswith(('/DEBUG', '/MAP', '/PDB:')) or value.startswith('/Fd'))
    if configuration == 'Release' and diagnostic and value.upper() != '/DEBUG:NONE':
        predicate = f'$<AND:{predicate},$<BOOL:${{CSE_RELEASE_DIAGNOSTICS}}>>'
    return predicate


def emit_list(lines, command, target, configuration, values, language=False):
    for value in values:
        value = portable(value)
        dependency_include = '${CSE_SDK_DIR}/external/installed/x86-windows-static-md/include'
        if command == 'target_include_directories' and (value == dependency_include or
                value.startswith(dependency_include + '/')):
            lines.append(f'cse_target_dependency_include({target} {configuration} {quote(value)})')
            continue
        predicate = option_predicate(configuration, value)
        if language:
            predicate = f'$<AND:{predicate},$<COMPILE_LANGUAGE:C,CXX>>'
        lines.append(f'{command}({target} PRIVATE {quote("$<" + predicate + ":" + value + ">")})')


def cmake_source(item):
    if item.get('generated'):
        return '${CMAKE_CURRENT_BINARY_DIR}/generated/' + Path(item['path']).name
    return portable(item['path'])


def emit_native_module_aliases(lines, target, configs):
    selections = [split(configs[name]['properties'].get('CseNativeModuleAliases', ''))
                  for name in ('Debug', 'Release')]
    if selections[0] != selections[1]:
        raise ValueError('Native module aliases differ between configurations')
    aliases = selections[0]
    if not aliases:
        return
    if any(config['properties']['ConfigurationType'] != 'DynamicLibrary' for config in configs.values()):
        raise ValueError('Native module aliases require a shared library')
    if any(not re.fullmatch(r'[A-Za-z_][A-Za-z0-9_-]*', alias) for alias in aliases):
        raise ValueError('Native module aliases must be plain module names')
    if len({alias.casefold() for alias in aliases}) != len(aliases):
        raise ValueError('Native module aliases must be unique')
    lines.append('if(NOT WIN32)')
    for alias in aliases:
        lines.append(f'  cse_native_module_alias({target} {quote(alias)})')
    lines.append('endif()')


def emit_native_sdl3_dependency(lines, target, configs):
    """Keep native-only SDL imports and usage includes outside Win32 selections."""
    values = [configs[name]['properties'].get('CseNativeSdl3', 'false')
              for name in ('Debug', 'Release')]
    if any(value not in ('true', 'false') for value in values):
        raise ValueError('CseNativeSdl3 must be true or false')
    if values[0] != values[1]:
        raise ValueError('Native SDL dependency differs between configurations')
    if values[0] == 'true':
        lines.append('if(NOT WIN32)')
        for configuration in ('Debug', 'Release'):
            lines.append(f'  cse_link_sdl3({target} {configuration})')
        lines.append('endif()')


def emit_mac_software_lifetime(lines, target, configs):
    """Only the owning swds module may select its qualified private clipboard registry."""
    values = [configs[config]['properties'].get('CseMacSoftwareLifetime', '') for config in ('Debug', 'Release')]
    if values == ['', '']:
        return
    if target != 'swds' or values != ['1', '1']:
        raise ValueError('CseMacSoftwareLifetime revision 1 requires the exact swds Debug/Release owner')
    paths = ('common/TextEntryClipboard.cpp', 'engine/native_software_lifetime.cpp')
    for config in ('Debug', 'Release'):
        for relative in paths:
            matches = [item for item in configs[config]['items']
                       if item['kind'] == 'ClCompile' and Path(item['path']) == SDK / relative]
            if len(matches) != 1 or matches[0]['metadata'].get('ExceptionHandling') != 'Sync':
                raise ValueError('Software retirement requires both actual Sync owning translation units')
            metadata = matches[0]['metadata']
            if relative.startswith('engine/') and (metadata.get('CseTargetOS') != 'macos'
                    or metadata.get('CsePointerWidth') != '64' or metadata.get('ExcludedFromBuild') != 'true'):
                raise ValueError('Software terminal owner must be a mandatory macOS-native64 member')
    lines.append('if(APPLE AND CMAKE_SIZEOF_VOID_P EQUAL 8)')
    for relative in paths:
        lines.append('  set_property(SOURCE "${CSE_SDK_DIR}/' + relative +
                     '" APPEND PROPERTY COMPILE_DEFINITIONS CSE_TEXT_ENTRY_USE_PTHREAD_KEY=1)')
    lines.append('endif()')


def emit_platform_sources(lines, target, configs):
    """Manifest-owned OS/width additions remain outside reference Win32 selections."""
    predicates = {'macos': 'APPLE', 'windows': 'WIN32', 'linux': 'CMAKE_SYSTEM_NAME STREQUAL "Linux"',
                  'posix': 'NOT WIN32'}
    selections = []
    selected_items = {}
    for configuration in ('Debug', 'Release'):
        selected = []
        selected_items[configuration] = {}
        reference_sources = {cmake_source(item) for item in configs[configuration]['items']
                             if item['kind'] == 'ClCompile'
                             and item['metadata'].get('ExcludedFromBuild') != 'true'}
        for item in configs[configuration]['items']:
            metadata = item['metadata']
            platform = metadata.get('CseTargetOS')
            width = metadata.get('CsePointerWidth')
            objective_arc = metadata.get('CseObjectiveCArc')
            if objective_arc is not None and (objective_arc != 'true' or platform != 'macos'
                                               or Path(item['path']).suffix != '.mm'):
                raise ValueError('CseObjectiveCArc requires an explicitly selected macos .mm source')
            apple_framework = metadata.get('CseAppleFramework')
            if apple_framework is not None and (platform != 'macos' or apple_framework != 'Cocoa'):
                raise ValueError('Unsupported CseAppleFramework platform dependency')
            if not platform and not width:
                if metadata.get('CseReplaces'):
                    raise ValueError('CseReplaces requires platform-only source ownership')
                continue
            if item['kind'] != 'ClCompile' or (platform and platform not in predicates):
                raise ValueError('Unsupported CseTargetOS source metadata: ' + str(platform))
            if width not in (None, '64'):
                raise ValueError('Unsupported CsePointerWidth source metadata: ' + str(width))
            if metadata.get('ExcludedFromBuild') != 'true':
                raise ValueError('Platform-only source must be excluded from reference Win32 selection')
            service = metadata.get('CseNativeServices', '')
            if service in ('fonts', 'font-registration-owner', 'voice-mixer'):
                valid_service = platform == 'macos'
            elif service == 'browser':
                valid_service = width == '64'
            else:
                valid_service = service == ''
            if not valid_service:
                raise ValueError('Unsupported CseNativeServices source metadata: ' + service)
            path = cmake_source(item)
            replacements = tuple(portable(str(source_path(value, ROOT)))
                                 for value in split(metadata.get('CseReplaces', '')))
            if any(value not in reference_sources for value in replacements):
                raise ValueError('CseReplaces must name an active reference source: ' + str(replacements))
            selected.append((platform, width, path, service, replacements))
            selected_items[configuration][path] = item
        selections.append(selected)
    if selections[0] != selections[1]:
        raise ValueError('Platform source membership differs between configurations')
    for platform in (*predicates, None):
        for width in (None, '64'):
            selected = [(path, service, replacements) for os_name, bits, path, service, replacements in selections[0]
                        if os_name == platform and bits == width]
            if not selected:
                continue
            predicate = predicates.get(platform, '')
            if width:
                predicate += (' AND ' if predicate else '') + 'CMAKE_SIZEOF_VOID_P EQUAL 8'
            lines.append('if(' + predicate + ')')
            for path, service, replacements in selected:
                if service == 'font-registration-owner':
                    lines.append(f'  cse_define_mac_font_registration_service({quote(path)})')
            for path, service, replacements in selected:
                for replaced in replacements:
                    # Source properties are scoped to this target's directory.
                    # Keep the reference inventory while compiling exactly one backend.
                    lines.append(f'  set_property(SOURCE {quote(replaced)} PROPERTY HEADER_FILE_ONLY TRUE)')
                if service != 'font-registration-owner':
                    lines.append(f'  target_sources({target} PRIVATE {quote(path)})')
                    objective_arc = selected_items['Debug'][path]['metadata'].get('CseObjectiveCArc') == 'true'
                    if objective_arc != (selected_items['Release'][path]['metadata'].get('CseObjectiveCArc') == 'true'):
                        raise ValueError('CseObjectiveCArc policy differs between configurations')
                    if objective_arc:
                        lines.append(f'  set_property(TARGET {target} PROPERTY OBJCXX_STANDARD 17)')
                        lines.append(f'  set_property(TARGET {target} PROPERTY OBJCXX_STANDARD_REQUIRED YES)')
                        lines.append(f'  set_property(TARGET {target} PROPERTY OBJCXX_EXTENSIONS NO)')
                    apple_framework = selected_items['Debug'][path]['metadata'].get('CseAppleFramework')
                    if apple_framework != selected_items['Release'][path]['metadata'].get('CseAppleFramework'):
                        raise ValueError('CseAppleFramework policy differs between configurations')
                    if apple_framework:
                        lines.append(f'  target_link_libraries({target} PRIVATE "-framework {apple_framework}")')
                    # These settings belong to the newly selected source, not its whole target.
                    for configuration in ('Debug', 'Release'):
                        overrides = selected_items[configuration][path].get('overrides', {})
                        flags = options(overrides, COMPILE) + additional_options(overrides.get('AdditionalOptions', ''))
                        if flags or objective_arc:
                            lines.append('  if(MSVC)')
                            for flag in flags:
                                condition = option_predicate(configuration, flag)
                                value = quote('$<' + condition + ':' + portable(flag) + '>')
                                lines.append(f'    set_property(SOURCE {quote(path)} APPEND PROPERTY COMPILE_OPTIONS {value})')
                            lines.append('  else()')
                            policy = portable_game_settings(selected_items[configuration][path]['metadata'])
                            for flag in policy['common'] + policy['cxx']:
                                condition = option_predicate(configuration, flag)
                                value = quote('$<' + condition + ':' + portable(flag) + '>')
                                lines.append(f'    set_property(SOURCE {quote(path)} APPEND PROPERTY COMPILE_OPTIONS {value})')
                            lines.append('  endif()')
            if any(service == 'fonts' for _, service, _ in selected):
                lines.append(f'  cse_link_native_font_services({target})')
            if any(service == 'browser' for _, service, _ in selected):
                lines.append(f'  cse_configure_native_browser({target})')
            if any(service == 'voice-mixer' for _, service, _ in selected):
                lines.append(f'  cse_link_native_voice_mixer({target})')
            lines.append('endif()')


def emit_portable_game_policy(lines, target, configs, project):
    """Compiler/OS policy only; source membership is emitted once in Sources.cmake."""
    lines.append(f'target_compile_features({target} PRIVATE cxx_std_17)')
    # The retained Windows CRT names are owned by one tested POSIX boundary.
    # Group -include with its operand so CMake option de-duplication cannot
    # detach another per-file forced include from this header.
    forced_include = quote('SHELL:-include "${CSE_SDK_DIR}/public/cse_posix_crt.h"')
    lines.append(f'target_compile_options({target} PRIVATE {forced_include})')
    lines.extend(['if(APPLE)', f'  target_compile_definitions({target} PRIVATE OSX POSIX)', 'endif()'])
    if any(data['properties'].get('WholeProgramOptimization') == 'true' for data in configs.values()):
        lines.append(f'target_compile_options({target} PRIVATE "$<$<CONFIG:Release>:-flto>")')
        lines.append(f'target_link_options({target} PRIVATE "$<$<CONFIG:Release>:-flto>")')
    lines.extend(['if(APPLE)', f'  target_link_options({target} PRIVATE "-Wl,-no_deduplicate")', 'endif()'])
    semantic_keys = {'ExceptionHandling', 'FloatingPointModel', 'RuntimeTypeInfo', 'Optimization',
                     'LanguageStandard', 'AdditionalOptions', 'DebugInformationFormat'}
    for config, data in configs.items():
        metadata = data['definitions']['ClCompile']
        policy = portable_game_settings(metadata)
        emit_list(lines, 'target_compile_options', target, config, policy['common'], language=True)
        if policy['symbols']:
            if config == 'Debug':
                emit_list(lines, 'target_compile_options', target, config, ['-g'], language=True)
            else:
                lines.append(f'target_compile_options({target} PRIVATE '
                             '"$<$<AND:$<CONFIG:Release>,$<BOOL:${CSE_RELEASE_DIAGNOSTICS}>>:-g>")')
        for option in policy['cxx']:
            predicate = f'$<AND:$<CONFIG:{config}>,$<COMPILE_LANGUAGE:CXX>>'
            lines.append(f'target_compile_options({target} PRIVATE {quote("$<" + predicate + ":" + option + ">")})')
        emit_list(lines, 'target_compile_definitions', target, config, policy['definitions'])
        includes = [str(source_path(path, project.parent))
                    for path in split(metadata.get('AdditionalIncludeDirectories', ''))]
        includes = [path for path in includes if path != data.get('mpg123', {}).get('include_root')]
        emit_list(lines, 'target_include_directories', target, config, includes)
        link = data['definitions'].get('Link', {})
        lib_dirs = [source_path(path, project.parent)
                    for path in split(link.get('AdditionalLibraryDirectories', ''))]
        libraries = split(link.get('AdditionalDependencies', '')) + [
            item['path'] for item in data['items'] if item['kind'] == 'Library']
        for library in libraries:
            leaf = Path(library.replace('\\', '/')).stem.lower()
            if leaf in SYSTEM_LIBS and '/' not in library and '\\' not in library:
                if leaf == 'opengl32':
                    emit_list(lines, 'target_link_libraries', target, config, ['-framework OpenGL'])
                continue
            if '/' in library or '\\' in library:
                path = source_path(library, project.parent)
            else:
                matches = [source_path(library, directory) for directory in lib_dirs
                           if source_path(library, directory).is_file()]
                if not matches:
                    raise ValueError('Unresolved portable library: ' + library)
                path = matches[0]
            lines.append(import_command(target, config, path, data))
        for item in data['items']:
            if item['kind'] != 'ClCompile' or item['metadata'].get('ExcludedFromBuild') == 'true':
                continue
            overrides = item['overrides']
            path = quote(cmake_source(item))
            if semantic_keys.intersection(overrides):
                effective = portable_game_settings(item['metadata'])
                for option in effective['common'] + effective['cxx']:
                    lines.append(f'set_property(SOURCE {path} APPEND PROPERTY COMPILE_OPTIONS '
                                 f'{quote("$<$<CONFIG:" + config + ">:" + option + ">")})')
            if ('DebugInformationFormat' in overrides or 'AdditionalOptions' in overrides):
                symbols = portable_game_settings(item['metadata'])['symbols']
                if symbols:
                    condition = f'$<CONFIG:{config}>'
                    if config == 'Release':
                        condition = '$<AND:$<CONFIG:Release>,$<BOOL:${CSE_RELEASE_DIAGNOSTICS}>>'
                    lines.append(f'set_property(SOURCE {path} APPEND PROPERTY COMPILE_OPTIONS '
                                 f'{quote("$<" + condition + ":-g>")})')
            if overrides.get('CompileAs') == 'CompileAsCpp' and config == 'Debug':
                lines.append(f'set_property(SOURCE {path} PROPERTY LANGUAGE CXX)')
            for include in split(overrides.get('ForcedIncludeFiles', '')):
                include = portable(str(source_path(include, project.parent)))
                for option in ('-include', include):
                    lines.append(f'set_property(SOURCE {path} APPEND PROPERTY COMPILE_OPTIONS '
                                 f'{quote("$<$<CONFIG:" + config + ">:" + option + ">")})')
            if 'PreprocessorDefinitions' in overrides:
                for value in portable_game_settings(item['metadata'])['definitions']:
                    lines.append(f'set_property(SOURCE {path} APPEND PROPERTY COMPILE_DEFINITIONS '
                                 f'{quote("$<$<CONFIG:" + config + ">:" + value + ">")})')
            if 'AdditionalIncludeDirectories' in overrides:
                for include in split(item['metadata']['AdditionalIncludeDirectories']):
                    include = portable(str(source_path(include, project.parent)))
                    lines.append(f'set_property(SOURCE {path} APPEND PROPERTY INCLUDE_DIRECTORIES '
                                 f'{quote("$<$<CONFIG:" + config + ">:" + include + ">")})')


def main():
    validate_payload(SDK / 'external/SDL3')
    records = []
    overview = ['# MSBuild to CMake crosswalk', '',
                'Generated by `python3 tools/translate_msbuild.py` from retained repository manifests.',
                'Repository imports, conditions and per-file metadata are expanded for Debug/Release Win32.',
                'This source crosswalk does not perform Microsoft toolchain/user-default evaluation or compilation.',
                'See [validation status](validation.md) for frozen cohorts and remaining native acceptance gates.',
                'The original byte-for-byte manifests are preserved in `msbuild/upstream-manifests.zip`.', '',
                'Each target has its own CMake directory, so source-specific settings cannot leak across targets.',
                'Exact selections and metadata are in `msbuild/repository-selections.json`; generated target modules are under `cmake/targets/`.', '',
                '| Project | CMake target | C/C++ | CRT Debug / Release | Release NDEBUG |',
                '| --- | --- | ---: | --- | --- |']
    for project in sorted(SDK.rglob('*.vcxproj')):
        if 'external' in project.relative_to(SDK).parts:
            continue
        target = TARGETS[project.stem]
        configs = {c: evaluate(project, c) for c in ['Debug', 'Release']}
        record = {'project': str(project.relative_to(SDK)), 'target': target, 'configurations': configs}
        records.append(record)
        debug, release = configs['Debug'], configs['Release']
        def compiled(c):
            return [i for i in c['items'] if i['kind'] == 'ClCompile' and i['metadata'].get('ExcludedFromBuild') != 'true']
        def selection(item):
            return ('generated', Path(item['path']).name, item['generated']['section']) if item.get('generated') else item['path']
        assert [selection(i) for i in compiled(debug)] == [selection(i) for i in compiled(release)], project
        active = compiled(release)
        if target in ('hw_engine', 'swds', 'hl_server', 'cs_server') and any(
                Path(i['path']).name in ('sys_dll.cpp', 'saved_callbacks.cpp') for i in active):
            generate_game_module_layouts(ROOT)
        if target in ('hl_server', 'cs_server'):
            generate_callback_catalog(ROOT, target, [Path(i['path']) for i in active])
        if target in ('hw_engine', 'swds', 'hl_client', 'cs_client') and any(
                Path(i['path']).name == 'cdll_int.cpp' for i in active):
            generate_client_module_layouts(ROOT)
        output = release['properties'].get('TargetName', project.stem)
        kind = 'EXECUTABLE' if release['properties']['ConfigurationType'] == 'Application' else 'SHARED'
        lines = ['# Generated from ' + str(project.relative_to(SDK)) + '; see tools/translate_msbuild.py.',
                 f'cse_add_runtime({target} {kind} {quote(output)})']
        for item in active:
            if item.get('generated'):
                assert Path(item['generated']['include']).is_file()
                lines.append(f'cse_a3d_geometry_source({quote(Path(item["path"]).name)} {quote(item["generated"]["section"])})')
        lines.append(f'target_sources({target} PRIVATE')
        for item in release['items']:
            if item['kind'] in ['ClCompile', 'ResourceCompile'] and item['metadata'].get('ExcludedFromBuild') != 'true':
                if not item.get('generated'):
                    assert Path(item['path']).is_file(), item['path']
                if item['kind'] == 'ClCompile':
                    lines.append('  ' + quote(cmake_source(item)))
        lines.append(')')
        resources = [item for item in release['items'] if item['kind'] == 'ResourceCompile'
                     and item['metadata'].get('ExcludedFromBuild') != 'true']
        if resources:
            lines.extend(['if(WIN32)', f'  target_sources({target} PRIVATE'])
            lines.extend('    ' + quote(cmake_source(item)) for item in resources)
            lines.extend(['  )', 'endif()'])
        emit_platform_sources(lines, target, configs)
        emit_mac_software_lifetime(lines, target, configs)
        portable_policy = True
        if portable_policy:
            sources = ROOT / 'cmake/targets' / target / 'Sources.cmake'
            sources.parent.mkdir(parents=True, exist_ok=True)
            sources.write_text(lines[0] + '\n' + '\n'.join(lines[2:]) + '\n')
            lines = lines[:2] + ['include("${CMAKE_CURRENT_LIST_DIR}/Sources.cmake")', 'if(MSVC)']
        for config, data in configs.items():
            m = data['definitions']['ClCompile']; link = data['definitions'].get('Link', {})
            overridden_flags = {k for item in compiled(data) for k in item['overrides'] if k in COMPILE}
            common_flags = {k: v for k, v in m.items() if k not in overridden_flags}
            flags = options(common_flags, COMPILE) + additional_options(m.get('AdditionalOptions', ''))
            if data['properties'].get('WholeProgramOptimization') == 'true':
                flags.append('/GL')
            if data['properties'].get('CharacterSet') == 'MultiByte':
                flags.append('/D_MBCS')
            if m.get('MultiProcessorCompilation') == 'true':
                flags.append('/MP' + m.get('ProcessorNumber', ''))
            if m.get('EnablePREfast') == 'true':
                flags.append('/analyze')
            if m.get('EnablePREfast') == 'true' and m.get('DisableAnalyzeExternal') == 'true':
                flags.append('/analyze:external-')
            emit_list(lines, 'target_compile_options', target, config, flags, language=True)
            if m.get('LanguageStandard'):
                std = {'stdcpp14': '14', 'stdcpp17': '17'}[m['LanguageStandard']]
                lines.append(f'set_property(TARGET {target} PROPERTY CXX_STANDARD {std})')
            emit_list(lines, 'target_compile_definitions', target, config, split(m.get('PreprocessorDefinitions', '')))
            includes = [str(source_path(p, project.parent)) for p in split(m.get('AdditionalIncludeDirectories', ''))]
            includes = [p for p in includes if p != data.get('mpg123', {}).get('include_root')]
            emit_list(lines, 'target_include_directories', target, config, includes)
            crt = {'MultiThreadedDLL': 'MultiThreadedDLL', 'MultiThreadedDebugDLL': 'MultiThreadedDebugDLL',
                   'MultiThreaded': 'MultiThreaded', 'MultiThreadedDebug': 'MultiThreadedDebug'}[m['RuntimeLibrary']]
            lines.append(f'set_property(TARGET {target} APPEND_STRING PROPERTY MSVC_RUNTIME_LIBRARY {quote("$<$<CONFIG:" + config + ">:" + crt + ">")})')
            link_flags = options(link, LINK) + additional_options(link.get('AdditionalOptions', ''))
            if data['properties'].get('WholeProgramOptimization') == 'true' and '/LTCG' not in link_flags:
                link_flags.append('/LTCG')
            incremental = data['properties'].get('LinkIncremental', 'true') if config == 'Debug' else 'false'
            link_flags.append('/INCREMENTAL' if incremental == 'true' else '/INCREMENTAL:NO')
            if link.get('BaseAddress'):
                link_flags.append('/BASE:' + link['BaseAddress'])
            if link.get('EnableUAC') == 'true':
                link_flags.append("/MANIFESTUAC:level='asInvoker'")
            if data['properties'].get('GenerateManifest') == 'false':
                link_flags.append('/MANIFEST:NO')
            if link.get('GenerateMapFile') == 'true':
                link_flags.append('/MAP:${CMAKE_BINARY_DIR}/symbols/' + config + '/' + target + '.map')
            emit_list(lines, 'target_link_options', target, config, link_flags)
            if link.get('ModuleDefinitionFile'):
                definition = source_path(link['ModuleDefinitionFile'], project.parent)
                assert definition.is_file(), definition
                if config == 'Debug':
                    if link.get('CseNativeCppExports') not in (None, 'true'):
                        raise ValueError('Unsupported native C++ export policy: ' + link['CseNativeCppExports'])
                    if target in ('hl_server', 'cs_server') or link.get('CseNativeCppExports') == 'true':
                        native = ROOT / 'cmake/exports' / (target + '.def')
                        native.parent.mkdir(parents=True, exist_ok=True)
                        native.write_text('; Generated by tools/translate_msbuild.py from ' +
                                          str(definition.relative_to(SDK)) + '\n' +
                                          native_definition(definition.read_text()))
                        lines.append(f'cse_game_module_exports({target} {quote(portable(str(definition)))} '
                                     f'"${{CSE_ROOT}}/cmake/exports/{target}.def")')
                    elif link.get('CseModuleDefinitionArchitecture') == 'x86':
                        lines.append(f'cse_win32_module_definition({target} {quote(portable(str(definition)))})')
                    else:
                        if link.get('CseModuleDefinitionArchitecture'):
                            raise ValueError('Unsupported module definition architecture: ' +
                                             link['CseModuleDefinitionArchitecture'])
                        lines.append(f'target_sources({target} PRIVATE {quote(portable(str(definition)))})')
            lib_dirs = [source_path(p, project.parent) for p in split(link.get('AdditionalLibraryDirectories', ''))]
            libraries = split(link.get('AdditionalDependencies', '')) + [i['path'] for i in data['items'] if i['kind'] == 'Library']
            for lib in libraries:
                leaf = Path(lib.replace('\\', '/')).stem.lower()
                if leaf in SYSTEM_LIBS and '/' not in lib and '\\' not in lib:
                    emit_list(lines, 'target_link_libraries', target, config, [leaf + '.lib'])
                else:
                    if '/' in lib or '\\' in lib:
                        path = source_path(lib, project.parent)
                    else:
                        matches = [source_path(lib, d) for d in lib_dirs if source_path(lib, d).is_file()]
                        if not matches:
                            raise ValueError('Unresolved library ' + lib + ' in ' + str(project))
                        path = matches[0]
                    assert path.is_file() or str(path) == data.get('mpg123', {}).get('import_library'), path
                    lines.append(import_command(target, config, path, data))
            for item in compiled(data):
                path = quote(cmake_source(item)); overrides = item['overrides']
                effective_flags = {k: v for k, v in item['metadata'].items() if k in overridden_flags}
                opts = options(effective_flags, COMPILE) + additional_options(overrides.get('AdditionalOptions', ''))
                if overrides.get('CompileAs') == 'CompileAsCpp':
                    opts.append('/TP')
                    if config == 'Debug':
                        lines.append(f'set_property(SOURCE {path} PROPERTY LANGUAGE CXX)')
                opts += ['/FI' + str(source_path(p, project.parent)) for p in split(overrides.get('ForcedIncludeFiles', ''))]
                for option in opts:
                    predicate = option_predicate(config, option)
                    lines.append(f'set_property(SOURCE {path} APPEND PROPERTY COMPILE_OPTIONS {quote("$<" + predicate + ":" + portable(option) + ">")})')
                if 'PreprocessorDefinitions' in overrides:
                    # Effective metadata already incorporates target definitions and inherited definitions.
                    values = split(item['metadata']['PreprocessorDefinitions'])
                    for value in values:
                        lines.append(f'set_property(SOURCE {path} APPEND PROPERTY COMPILE_DEFINITIONS {quote("$<$<CONFIG:" + config + ">:" + value + ">")})')
                if 'AdditionalIncludeDirectories' in overrides:
                    # Per-file paths must precede target includes; MSBuild uses the same ordering.
                    paths = [portable(str(source_path(p, project.parent))) for p in split(item['metadata']['AdditionalIncludeDirectories'])]
                    for inc in paths:
                        dependency_include = '${CSE_SDK_DIR}/external/installed/x86-windows-static-md/include'
                        if inc == dependency_include or inc.startswith(dependency_include + '/'):
                            lines.append(f'cse_source_dependency_include({path} {config} {quote(inc)})')
                        else:
                            lines.append(f'set_property(SOURCE {path} APPEND PROPERTY INCLUDE_DIRECTORIES {quote("$<$<CONFIG:" + config + ">:" + inc + ">")})')
        if portable_policy:
            lines.append('else()')
            emit_portable_game_policy(lines, target, configs, project)
            lines.append('endif()')
        for item in release['items']:
            if item['kind'] == 'ProjectReference':
                dep = TARGETS[Path(item['path']).stem]
                lines.append(f'target_link_libraries({target} PRIVATE {dep})')
        subdir = {'cs_client': 'cstrike/cl_dlls', 'cs_server': 'cstrike/dlls',
                  'hl_client': 'valve/cl_dlls', 'hl_server': 'valve/dlls',
                  'gameui': 'valve/cl_dlls', 'particleman': 'valve/cl_dlls'}.get(target, '')
        record['runtime'] = {'kind': kind, 'name': output, 'directory': subdir}
        lines.append(f'cse_runtime_directory({target} {quote(subdir)})')
        emit_native_module_aliases(lines, target, configs)
        emit_native_sdl3_dependency(lines, target, configs)
        if target == 'vgui':
            lines.append('cse_vgui_import_archive()')
        dest = ROOT / 'cmake/targets' / target / 'CMakeLists.txt'
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_text('\n'.join(lines) + '\n')
        crts = [c['definitions']['ClCompile']['RuntimeLibrary'] for c in [debug, release]]
        ndebug = 'yes' if 'NDEBUG' in split(release['definitions']['ClCompile'].get('PreprocessorDefinitions', '')) else 'no'
        overview.append(f'| `{project.relative_to(SDK)}` | `{target}` | {len(active)} | {crts[0]} / {crts[1]} | {ndebug} |')
    # Store target defaults once and only effective per-file differences. Avoid
    # repeating every inherited compiler setting for every source/header item.
    for record in records:
        for data in record['configurations'].values():
            data['items'] = [dict(kind=item['kind'], path=item['path'],
                                  metadata={k: item['metadata'][k] for k in item['overrides']},
                                  **({'generated': item['generated']} if item.get('generated') else {}))
                             for item in data['items']
                             if item['kind'] in ['ClCompile', 'ResourceCompile', 'Library', 'ProjectReference']]
    evidence = json.dumps(records, indent=2).replace(str(SDK), '${CSE_SDK_DIR}')
    (ROOT / 'docs/msbuild/repository-selections.json').write_text(evidence + '\n')
    (ROOT / 'docs/msbuild-crosswalk.md').write_text('\n'.join(overview) + '\n')
    print('Translated', len(records), 'retained/optional projects and both configurations.')


if __name__ == '__main__':
    main()
