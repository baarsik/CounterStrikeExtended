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
from cmake_inventory import query_target


class NativeRuntimePolicyTests(unittest.TestCase):
    def test_native_base_replacements_preserve_semantic_policy(self):
        owners = ('hw_engine', 'swds', 'gameui', 'hltv', 'hltv_core', 'hltv_proxy', 'demoplayer')
        for target in owners:
            for configuration in ('Debug', 'Release'):
                reference = query_target(ROOT, target, configuration)
                reference_source = next(item for item in reference['sources']
                                        if item['language'] == 'CXX' and item['path'].endswith('/BaseSystemModule.cpp'))
                for target_os, architecture in (('windows', 'x64'), ('macos', 'arm64')):
                    with self.subTest(target=target, configuration=configuration, target_os=target_os):
                        native = query_target(ROOT, target, configuration, target_os, architecture)
                        selected = [item for item in native['sources']
                                    if item['language'] == 'CXX' and item['path'].endswith('/BaseSystemModule.cpp')]
                        self.assertEqual(len(selected), 1)
                        self.assertEqual(selected[0]['path'], str(ROOT / 'src/common/native/BaseSystemModule.cpp'))
                        flags = selected[0]['compile_options']
                        if target_os == 'windows':
                            semantic = lambda options: [flag for flag in options
                                if flag.startswith(('/fp:', '/EH', '/GR'))]
                            self.assertEqual(semantic(flags), semantic(reference_source['compile_options']))
                            self.assertEqual(native['properties']['MSVC_RUNTIME_LIBRARY'],
                                             reference['properties']['MSVC_RUNTIME_LIBRARY'])
                            self.assertFalse(any(flag.startswith('/arch:') for flag in flags))
                        else:
                            self.assertIn('-ffp-model=precise', flags)
                            self.assertIn('-ffp-contract=off', flags)
                            self.assertIn('-fexceptions' if '/EHsc' in reference_source['compile_options']
                                          else '-fno-exceptions', flags)
                        self.assertEqual(native['properties']['CXX_STANDARD'],
                                         reference['properties']['CXX_STANDARD'] if target_os == 'windows' else '17')

    def test_machine_and_x86_only_flags_follow_the_selected_target(self):
        for architecture in ('x86', 'x64', 'arm64'):
            for configuration in ('Debug', 'Release'):
                data = query_target(ROOT, 'hw_engine', configuration, 'windows', architecture)
                machine = query_target(ROOT, 'gameui', configuration, 'windows', architecture)
                self.assertIn('/MACHINE:' + architecture.upper(), machine['link_options'])
                preferred = query_target(ROOT, 'a3dapi', configuration, 'windows', architecture)
                self.assertEqual(any(flag.startswith('/BASE:') for flag in preferred['link_options']),
                                 architecture == 'x86')
                for source in data['sources']:
                    if source['language'] not in ('C', 'CXX'):
                        continue
                    self.assertEqual(any(flag.startswith('/arch:') for flag in source['compile_options']),
                                     architecture == 'x86')

    @unittest.skipIf(sys.platform == 'win32', 'requires the native Clang portable policy')
    def test_portable_compile_uses_the_owned_crt_boundary(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'ordinary.cpp').write_text('int main() { return _stricmp("CS", "cs"); }\n')
            for configuration in ('Release', 'Debug'):
                data = query_target(ROOT, 'cs_server', configuration, 'macos', 'arm64')
                source = next(item for item in data['sources'] if item['language'] == 'CXX')
                flags = source['compile_options']
                forced = flags.index('-include')
                header = flags[forced + 1]
                self.assertEqual(header, str(ROOT / 'src/public/cse_posix_crt.h'))
                # Compile and execute a minimal consumer with the actual selected
                # boundary, independently of external game/link dependencies.
                (root / 'CMakeLists.txt').write_text(
                    'cmake_minimum_required(VERSION 3.25)\nproject(PortableCrtPolicy CXX)\n'
                    'add_executable(fixture ordinary.cpp)\n'
                    f'target_compile_options(fixture PRIVATE -include "{header}")\n')
                build = root / configuration
                for command in (['cmake', '-S', str(root), '-B', str(build), '-G', 'Ninja',
                                 '-DCMAKE_BUILD_TYPE=' + configuration],
                                ['cmake', '--build', str(build)], [str(build / 'fixture')]):
                    result = subprocess.run(command, capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

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


if __name__ == '__main__':
    unittest.main()
