"""Evaluate target CPU policy without changing the retained source selections."""
from pathlib import Path
import json
import os
import re
import shlex
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import translate_msbuild as translator


class NativeRuntimePolicyTests(unittest.TestCase):
    def test_native_base_replacements_preserve_projected_reference_policy(self):
        # Evaluate the complete generated production target/source policy. Only
        # external dependency/link actions are stubbed; CMake still selects the
        # source and expands its actual target + per-file compiler options.
        owners = {'hw_engine', 'swds', 'gameui', 'hltv', 'hltv_core', 'hltv_proxy', 'demoplayer'}
        projects = {translator.TARGETS[path.stem]: path for path in translator.SDK.rglob('*.vcxproj')
                    if 'external' not in path.relative_to(translator.SDK).parts
                    and translator.TARGETS[path.stem] in owners}
        self.assertEqual(set(projects), owners)
        configs = {owner: {config: translator.evaluate(path, config) for config in ('Debug', 'Release')}
                   for owner, path in projects.items()}
        native = str(translator.SDK / 'common/native/BaseSystemModule.cpp')
        references = {}
        for owner, configurations in configs.items():
            for config, data in configurations.items():
                selected = [item for item in data['items'] if item['path'] == native]
                self.assertEqual(len(selected), 1)
                replaced = translator.split(selected[0]['metadata']['CseReplaces'])
                self.assertEqual(len(replaced), 1)
                reference = str(translator.source_path(replaced[0], ROOT))
                references[owner, config] = next(item for item in data['items'] if item['path'] == reference)
            # Fail when metadata was changed without regenerating the production selection.
            lines = []
            translator.emit_platform_sources(lines, owner, configurations)
            self.assertIn('\n'.join(lines), (ROOT / 'cmake/targets' / owner / 'Sources.cmake').read_text())

        def policy(tokens, msvc):
            groups = (('/fp:',), ('/EH',), ('/GR',), ('/O1', '/O2', '/Od'), ('/arch:',)) if msvc else (
                ('-ffp-model=', '-ffast-math'), ('-ffp-contract=',), ('-fexceptions', '-fno-exceptions'),
                ('-frtti', '-fno-rtti'), ('-O0', '-O2', '-Os'))
            return [next((token for token in reversed(tokens) if token.startswith(prefixes)), None)
                    for prefixes in groups]

        runtime_helpers = (ROOT / 'cmake/RuntimeTargets.cmake').read_text()
        add_runtime = re.search(r'function\(cse_add_runtime\b.*?endfunction\(\)', runtime_helpers, re.S)[0]
        helpers = ('cse_runtime_directory', 'cse_link_import', 'cse_link_steam', 'cse_link_sdl3',
                   'cse_link_mpg123', 'cse_target_dependency_include', 'cse_source_dependency_include',
                   'cse_game_module_exports', 'cse_configure_native_browser', 'cse_link_native_font_services',
                   'cse_link_native_voice_mixer', 'cse_define_mac_font_registration_service')
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory)
            root_script = [
                'cmake_minimum_required(VERSION 3.25)',
                f'set(CMAKE_USER_MAKE_RULES_OVERRIDE "{ROOT.as_posix()}/cmake/MSVCDefaults.cmake")',
                'project(NativeBasePolicy C CXX)',
                f'set(CSE_SDK_DIR "{translator.SDK.as_posix()}")',
                'set(MSVC ${CSE_TEST_MSVC})', 'set(WIN32 ${CSE_TEST_MSVC})',
                'set(APPLE ${CSE_TEST_APPLE})', 'set(CMAKE_SIZEOF_VOID_P ${CSE_TEST_POINTER_SIZE})',
                add_runtime]
            for helper in helpers:
                root_script += [f'function({helper})', 'endfunction()']
            for owner in sorted(owners):
                target_source = source / owner
                target_source.mkdir()
                (target_source / 'CMakeLists.txt').write_text(
                    f'include("{ROOT.as_posix()}/cmake/targets/{owner}/CMakeLists.txt")\n'
                    f'get_target_property(selected {owner} SOURCES)\n'
                    'list(FILTER selected INCLUDE REGEX "/BaseSystemModule[.]cpp$")\n'
                    f'set_property(TARGET {owner} PROPERTY SOURCES "${{selected}}")\n'
                    f'set_property(TARGET {owner} PROPERTY LINK_LIBRARIES "")\n'
                    f'file(GENERATE OUTPUT "${{CMAKE_BINARY_DIR}}/{owner}-properties.txt" CONTENT '
                    f'"$<TARGET_PROPERTY:{owner},CXX_STANDARD>|'
                    f'$<TARGET_GENEX_EVAL:{owner},$<TARGET_PROPERTY:{owner},MSVC_RUNTIME_LIBRARY>>")\n')
                root_script.append(f'add_subdirectory({owner})')
            (source / 'CMakeLists.txt').write_text('\n'.join(root_script) + '\n')
            # This is generation on the host compiler, never a Windows build.
            profiles = [('win32', 'x86', True, False, 4), ('windows-native', 'x64', True, False, 8),
                        ('macos-native', 'arm64', False, True, 8)]
            for profile, arch, msvc, apple, size in profiles:
                for config in ('Debug', 'Release'):
                    with self.subTest(profile=profile, config=config):
                        build = source / (profile + config)
                        result = subprocess.run(['cmake', '-S', str(source), '-B', str(build), '-G', 'Ninja',
                            '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON', f'-DCMAKE_BUILD_TYPE={config}',
                            f'-DCSE_TARGET_ARCH={arch}', f'-DCSE_TEST_MSVC={int(msvc)}',
                            f'-DCSE_TEST_APPLE={int(apple)}', f'-DCSE_TEST_POINTER_SIZE={size}'],
                            capture_output=True, text=True)
                        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                        commands = json.loads((build / 'compile_commands.json').read_text())
                        for owner in sorted(owners):
                            with self.subTest(owner=owner):
                                reference = references[owner, config]
                                rows = [row for row in commands
                                        if f'CMakeFiles/{owner}.dir/' in row['command'].replace('\\', '/')]
                                self.assertEqual(len(rows), 1, rows)
                                expected_source = reference['path'] if size == 4 else native
                                self.assertEqual(Path(rows[0]['file']), Path(expected_source))
                                metadata = reference['metadata']
                                if msvc:
                                    expected = translator.options(metadata, translator.COMPILE)
                                    if size != 4:
                                        expected = [flag for flag in expected if not flag.startswith('/arch:')]
                                    properties = (build / f'{owner}-properties.txt').read_text().split('|')
                                    self.assertEqual(properties, [metadata['LanguageStandard'].removeprefix('stdcpp'),
                                                                  metadata['RuntimeLibrary']])
                                else:
                                    portable = translator.portable_game_settings(metadata)
                                    expected = portable['common'] + portable['cxx']
                                    properties = (build / f'{owner}-properties.txt').read_text().split('|')
                                    self.assertEqual(properties, [str(portable['standard']), ''])
                                actual = policy(shlex.split(rows[0]['command']), msvc)
                                self.assertEqual(actual, policy(expected, msvc), rows[0]['command'])

    def test_native_vgui_keeps_native_exports_and_import_archive(self):
        reference = os.environ.get('CSE_TEST_POLICY_REFERENCE')
        def source(path):
            if reference:
                return subprocess.check_output(['git', 'show', reference + ':' + path], cwd=ROOT, text=True)
            return (ROOT / path).read_text()
        helpers = source('cmake/RuntimeTargets.cmake')
        definitions = source('cmake/targets/vgui/CMakeLists.txt')
        selected = [line for line in definitions.splitlines() if 'utils/vgui/vgui.def' in line]
        self.assertEqual(len(selected), 1)
        bodies = []
        for name in ('cse_vgui_import_archive', 'cse_win32_module_definition'):
            match = re.search(r'function\(' + name + r'\b.*?endfunction\(\)', helpers, re.S)
            if match:
                bodies.append(match[0])
        # Evaluate the generated selection and actual helpers; never execute a librarian or compiler.
        with tempfile.TemporaryDirectory() as directory:
            script = Path(directory) / 'policy.cmake'
            receipt = Path(directory) / 'receipt.txt'
            script.write_text('set(WIN32 TRUE)\n'
                'function(target_sources)\nfile(APPEND "${CSE_RECEIPT}" "definition;${ARGV}\\n")\nendfunction()\n'
                'function(add_custom_command)\nfile(APPEND "${CSE_RECEIPT}" "archive;${ARGV}\\n")\nendfunction()\n' +
                '\n'.join(bodies + selected) + '\ncse_vgui_import_archive()\n')
            for arch in ('x86', 'x64', 'arm64'):
                receipt.write_text('')
                result = subprocess.run(['cmake', f'-DCSE_TARGET_ARCH={arch}', f'-DCSE_RECEIPT={receipt}',
                                         '-P', str(script)], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                output = receipt.read_text()
                if arch == 'x86':
                    self.assertIn('vgui.def', output)
                    self.assertIn('/machine:x86', output)
                    self.assertIn('vgui_imports.def', output)
                else:
                    self.assertEqual(output, '')
        for config in ('Release', 'Debug'):
            exports = [line for line in definitions.splitlines() if '/EXPORT:VguiEmptyString,@395,DATA' in line
                       and f'$<CONFIG:{config}>' in line]
            self.assertEqual(len(exports), 1, 'VGUI empty-string DATA ordinal must survive native export selection')

    def test_apple_runtime_keeps_inline_registries_local_to_their_module(self):
        helpers = (ROOT / 'cmake/RuntimeTargets.cmake').read_text()
        body = re.search(r'function\(cse_add_runtime\b.*?endfunction\(\)', helpers, re.S)[0]
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory)
            (source / 'ordinary.cpp').write_text('int ordinary;\n')
            (source / 'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.25)\n'
                'project(InlineRegistryPolicy CXX)\nset(APPLE ${CSE_TEST_APPLE})\n' + body +
                '\ncse_add_runtime(fixture SHARED fixture)\ntarget_sources(fixture PRIVATE ordinary.cpp)\n')
            for apple in ('ON', 'OFF'):
                build = source / apple
                result = subprocess.run(['cmake', '-S', str(source), '-B', str(build), '-G', 'Ninja',
                    '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON', f'-DCSE_TEST_APPLE={apple}'], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                command = json.loads((build / 'compile_commands.json').read_text())[0]['command']
                self.assertEqual('-fvisibility-inlines-hidden' in command, apple == 'ON')

    @unittest.skipIf(sys.platform == 'win32', 'requires the native Clang portable policy')
    def test_portable_compile_uses_the_owned_crt_boundary(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory)
            (source / 'ordinary.cpp').write_text('int main() { return _stricmp("CS", "cs"); }\n')
            metadata = {'Optimization': 'MaxSpeed', 'TreatWarningAsError': 'true'}
            configs = {config: {'properties': {}, 'definitions': {'ClCompile': metadata}, 'items': []}
                       for config in ('Release', 'Debug')}
            lines = []
            translator.emit_portable_game_policy(lines, 'fixture', configs, ROOT / 'GoldSrc-SDK/fixture.vcxproj')
            (source / 'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.25)\n'
                f'set(CMAKE_USER_MAKE_RULES_OVERRIDE "{ROOT}/cmake/MSVCDefaults.cmake")\n'
                'project(PortableCrtPolicy CXX)\n'
                f'set(CSE_SDK_DIR "{ROOT}/GoldSrc-SDK")\n'
                'add_executable(fixture ordinary.cpp)\n' + '\n'.join(lines) + '\n')
            for configuration in ('Release', 'Debug'):
                build = source / configuration
                result = subprocess.run(['cmake', '-S', str(source), '-B', str(build), '-G', 'Ninja',
                    f'-DCMAKE_BUILD_TYPE={configuration}'], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                result = subprocess.run(['cmake', '--build', str(build)], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                result = subprocess.run([str(build / 'fixture')], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_macos_runtime_presets_keep_all_targets_and_isolated_outputs(self):
        presets = json.loads((ROOT / 'CMakePresets.json').read_text())
        configure = {item['name']: item for item in presets['configurePresets']}
        build = {item['name']: item for item in presets['buildPresets']}
        for configuration in ('Release', 'Debug'):
            name = 'runtime-macos-arm64-' + configuration.lower()
            item = configure[name]
            self.assertEqual(item['generator'], 'Ninja')
            self.assertEqual(item['binaryDir'], '${sourceDir}/build/' + name)
            self.assertEqual(build[name]['configurePreset'], name)
            values = item['cacheVariables']
            self.assertEqual(values['CSE_TARGET_ARCH'], 'arm64')
            self.assertEqual(values['CMAKE_OSX_ARCHITECTURES'], 'arm64')
            self.assertEqual(values['CMAKE_OSX_DEPLOYMENT_TARGET'], '14.0')
            self.assertEqual(values['CMAKE_BUILD_TYPE'], configuration)
            self.assertEqual(values['CSE_HARDWARE_ONLY'], 'OFF')
            for option in ('CSE_BUILD_DEDICATED', 'CSE_BUILD_BROADCASTING', 'CSE_BUILD_A3D', 'CSE_SDL3_FROM_SOURCE'):
                self.assertEqual(values[option], 'ON')
            self.assertNotIn('CSE_OUTPUT_DIRECTORY', values)
            self.assertEqual(item['condition']['rhs'], 'Darwin')
        self.assertEqual(configure['default']['inherits'], 'runtime-x86-release')

    def test_dependency_include_is_resolved_before_configuration_wrapping(self):
        lines = []
        path = str(ROOT / 'GoldSrc-SDK/external/installed/x86-windows-static-md/include')
        translator.emit_list(lines, 'target_include_directories', 'fixture', 'Release', [path])
        self.assertEqual(lines, ['cse_target_dependency_include(fixture Release '
            '"${CSE_SDK_DIR}/external/installed/x86-windows-static-md/include")'])

    def test_evaluated_machine_and_x86_only_flags_follow_selected_target(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory)
            (source / 'ordinary.cpp').write_text('int ordinary;\n')
            (source / 'override.cpp').write_text('int overridden;\n')
            lines = []
            for config in ('Release', 'Debug'):
                translator.emit_list(lines, 'target_compile_options', 'fixture', config,
                                     ['/arch:SSE2', '/fp:precise', '/EHs-c-'], language=True)
                translator.emit_list(lines, 'target_link_options', 'fixture', config,
                                     translator.options({'TargetMachine': 'MachineX86'}, translator.LINK) + ['/BASE:0x10000000'])
                predicate = translator.option_predicate(config, '/arch:IA32')
                lines.append('set_property(SOURCE override.cpp APPEND PROPERTY COMPILE_OPTIONS '
                             + translator.quote('$<' + predicate + ':/arch:IA32>') + ')')
            (source / 'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.25)\n'
                'project(TargetCpuPolicy CXX)\nadd_library(fixture STATIC ordinary.cpp override.cpp)\n'
                'string(TOUPPER "${CSE_TARGET_ARCH}" CSE_MSVC_MACHINE)\n' + '\n'.join(lines) + '\n'
                'file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/link-options.txt" '
                'CONTENT "$<TARGET_PROPERTY:fixture,LINK_OPTIONS>")\n')
            for arch in ('x86', 'x64', 'arm64'):
                for config in ('Release', 'Debug'):
                    build = source / (arch + config)
                    result = subprocess.run(['cmake', '-S', str(source), '-B', str(build), '-G', 'Ninja',
                        '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON', f'-DCSE_TARGET_ARCH={arch}',
                        f'-DCMAKE_BUILD_TYPE={config}'], capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    link = (build / 'link-options.txt').read_text()
                    self.assertEqual(link, '/MACHINE:' + arch.upper() + (';/BASE:0x10000000' if arch == 'x86' else ''))
                    for item in json.loads((build / 'compile_commands.json').read_text()):
                        command = item['command']
                        overridden = Path(item['file']).name == 'override.cpp'
                        self.assertEqual('/arch:SSE2' in command, arch == 'x86')
                        self.assertEqual('/arch:IA32' in command, arch == 'x86' and overridden)
                        self.assertIn('/fp:precise', command)
                        self.assertIn('/EHs-c-', command)


if __name__ == '__main__':
    unittest.main()
