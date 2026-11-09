"""Evaluate target CPU policy without changing the retained source selections."""
from pathlib import Path
import json
import os
import re
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import translate_msbuild as translator


class NativeRuntimePolicyTests(unittest.TestCase):
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
