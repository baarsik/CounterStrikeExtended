#!/usr/bin/env python3
"""Translate this fork's retained repository manifests; never invokes MSBuild.

Microsoft toolchain/user defaults are outside this source-only evaluator. The
original manifests remain archived, and actual Windows evaluation is separate.
"""
from pathlib import Path
import collections
import json
import os
import re
import xml.etree.ElementTree as ET

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
                    walk(ET.parse(path).getroot(), path)
            elif kind == 'ImportGroup':
                walk(element, current)
            elif kind == 'PropertyGroup':
                for node in element:
                    if condition(node.get('Condition')):
                        properties[name(node)] = expand((node.text or '').strip())
            elif kind == 'ItemDefinitionGroup':
                for node in element:
                    definitions[name(node)] = metadata(node, definitions.get(name(node), {}))
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
    return {'properties': properties, 'definitions': definitions, 'items': items, 'imports': imports}


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
        'TargetMachine': {'MachineX86': '/MACHINE:X86'},
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


def emit_list(lines, command, target, configuration, values, language=False):
    for value in values:
        value = portable(value)
        predicate = f'$<CONFIG:{configuration}>'
        if language:
            predicate = f'$<AND:{predicate},$<COMPILE_LANGUAGE:C,CXX>>'
        lines.append(f'{command}({target} PRIVATE {quote("$<" + predicate + ":" + value + ">")})')


def main():
    records = []
    overview = ['# MSBuild to CMake crosswalk', '',
                'Generated by `python3 tools/translate_msbuild.py` from retained repository manifests.',
                'Repository imports, conditions and per-file metadata are expanded for Debug/Release Win32.',
                '**Microsoft toolchain/user-default evaluation and compilation have not run.**',
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
        assert [i['path'] for i in compiled(debug)] == [i['path'] for i in compiled(release)], project
        active = compiled(release)
        output = release['properties'].get('TargetName', project.stem)
        kind = 'EXECUTABLE' if release['properties']['ConfigurationType'] == 'Application' else 'SHARED'
        lines = ['# Generated from ' + str(project.relative_to(SDK)) + '; see tools/translate_msbuild.py.',
                 f'cse_add_runtime({target} {kind} {quote(output)})', f'target_sources({target} PRIVATE']
        for item in release['items']:
            if item['kind'] in ['ClCompile', 'ResourceCompile'] and item['metadata'].get('ExcludedFromBuild') != 'true':
                assert Path(item['path']).is_file(), item['path']
                lines.append('  ' + quote(portable(item['path'])))
        lines.append(')')
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
            if m.get('DisableAnalyzeExternal') == 'true':
                flags.append('/analyze:external-')
            emit_list(lines, 'target_compile_options', target, config, flags, language=True)
            if m.get('LanguageStandard'):
                std = {'stdcpp14': '14', 'stdcpp17': '17'}[m['LanguageStandard']]
                lines.append(f'set_property(TARGET {target} PROPERTY CXX_STANDARD {std})')
            emit_list(lines, 'target_compile_definitions', target, config, split(m.get('PreprocessorDefinitions', '')))
            includes = [str(source_path(p, project.parent)) for p in split(m.get('AdditionalIncludeDirectories', ''))]
            emit_list(lines, 'target_include_directories', target, config, includes)
            crt = {'MultiThreadedDLL': 'MultiThreadedDLL', 'MultiThreadedDebugDLL': 'MultiThreadedDebugDLL',
                   'MultiThreaded': 'MultiThreaded', 'MultiThreadedDebug': 'MultiThreadedDebug'}[m['RuntimeLibrary']]
            lines.append(f'set_property(TARGET {target} APPEND_STRING PROPERTY MSVC_RUNTIME_LIBRARY {quote("$<$<CONFIG:" + config + ">:" + crt + ">")})')
            link_flags = options(link, LINK) + additional_options(link.get('AdditionalOptions', ''))
            if data['properties'].get('WholeProgramOptimization') == 'true' and '/LTCG' not in link_flags:
                link_flags.append('/LTCG')
            incremental = data['properties'].get('LinkIncremental', 'true' if config == 'Debug' else 'false')
            link_flags.append('/INCREMENTAL' if incremental == 'true' else '/INCREMENTAL:NO')
            if link.get('BaseAddress'):
                link_flags.append('/BASE:' + link['BaseAddress'])
            if link.get('EnableUAC') == 'true':
                link_flags.append('/MANIFESTUAC:level=asInvoker')
            if data['properties'].get('GenerateManifest') == 'false':
                link_flags.append('/MANIFEST:NO')
            if link.get('GenerateMapFile') == 'true':
                link_flags.append('/MAP:${CMAKE_BINARY_DIR}/symbols/' + config + '/' + target + '.map')
            emit_list(lines, 'target_link_options', target, config, link_flags)
            if link.get('ModuleDefinitionFile'):
                definition = source_path(link['ModuleDefinitionFile'], project.parent)
                assert definition.is_file(), definition
                lines.append(f'target_sources({target} PRIVATE {quote(portable(str(definition)))})') if config == 'Debug' else None
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
                    assert path.is_file(), path
                    lines.append(f'cse_link_import({target} {config} {quote(portable(str(path)))})')
            for item in compiled(data):
                path = quote(portable(item['path'])); overrides = item['overrides']
                effective_flags = {k: v for k, v in item['metadata'].items() if k in overridden_flags}
                opts = options(effective_flags, COMPILE) + additional_options(overrides.get('AdditionalOptions', ''))
                if overrides.get('CompileAs') == 'CompileAsCpp':
                    opts.append('/TP')
                opts += ['/FI' + str(source_path(p, project.parent)) for p in split(overrides.get('ForcedIncludeFiles', ''))]
                for option in opts:
                    lines.append(f'set_property(SOURCE {path} APPEND PROPERTY COMPILE_OPTIONS {quote("$<$<CONFIG:" + config + ">:" + portable(option) + ">")})')
                if 'PreprocessorDefinitions' in overrides:
                    # Effective metadata already incorporates target definitions and inherited definitions.
                    values = split(item['metadata']['PreprocessorDefinitions'])
                    for value in values:
                        lines.append(f'set_property(SOURCE {path} APPEND PROPERTY COMPILE_DEFINITIONS {quote("$<$<CONFIG:" + config + ">:" + value + ">")})')
                if 'AdditionalIncludeDirectories' in overrides:
                    # Per-file paths must precede target includes; MSBuild uses the same ordering.
                    paths = [portable(str(source_path(p, project.parent))) for p in split(item['metadata']['AdditionalIncludeDirectories'])]
                    for inc in paths:
                        lines.append(f'set_property(SOURCE {path} APPEND PROPERTY INCLUDE_DIRECTORIES {quote("$<$<CONFIG:" + config + ">:" + inc + ">")})')
        for item in release['items']:
            if item['kind'] == 'ProjectReference':
                dep = TARGETS[Path(item['path']).stem]
                lines.append(f'target_link_libraries({target} PRIVATE {dep})')
        subdir = {'cs_client': 'cstrike/cl_dlls', 'cs_server': 'cstrike/dlls',
                  'hl_client': 'valve/cl_dlls', 'hl_server': 'valve/dlls',
                  'gameui': 'valve/cl_dlls', 'particleman': 'valve/cl_dlls'}.get(target, '')
        lines.append(f'cse_runtime_directory({target} {quote(subdir)})')
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
                                  metadata={k: item['metadata'][k] for k in item['overrides']})
                             for item in data['items']
                             if item['kind'] in ['ClCompile', 'ResourceCompile', 'Library', 'ProjectReference']]
    evidence = json.dumps(records, indent=2).replace(str(SDK), '${CSE_SDK_DIR}')
    (ROOT / 'docs/msbuild/repository-selections.json').write_text(evidence + '\n')
    (ROOT / 'docs/msbuild-crosswalk.md').write_text('\n'.join(overview) + '\n')
    print('Translated', len(records), 'retained/optional projects and both configurations.')


if __name__ == '__main__':
    main()
