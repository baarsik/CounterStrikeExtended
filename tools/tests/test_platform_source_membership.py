"""Platform-only source ownership stays in retained manifests, outside compiler policy."""
from copy import deepcopy
from pathlib import Path
import sys
import unittest
import subprocess
import tempfile
import json
import os

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import translate_msbuild as translator


class PlatformSourceMembershipTests(unittest.TestCase):
    def fixture(self):
        item = {'kind': 'ClCompile', 'path': str(ROOT / 'GoldSrc-SDK/example.cpp'),
                'metadata': {'ExcludedFromBuild': 'true', 'CseTargetOS': 'macos'},
                'overrides': {'ExcludedFromBuild': 'true', 'CseTargetOS': 'macos'}}
        return {config: {'items': [deepcopy(item)]} for config in ('Debug', 'Release')}

    def test_explicit_platform_source_remains_conditional(self):
        lines = []
        translator.emit_platform_sources(lines, 'fixture', self.fixture())
        text = '\n'.join(lines)
        self.assertIn('if(APPLE)', text)
        self.assertIn('target_sources(fixture PRIVATE "${CSE_SDK_DIR}/example.cpp")', text)
        self.assertNotIn('/MACHINE', text)
        self.assertNotIn('target_compile_options', text)

    def test_objc_arc_is_private_to_an_explicit_macos_mm_source(self):
        configs = self.fixture()
        for config in configs.values():
            item = config['items'][0]
            item['path'] = str(ROOT / 'GoldSrc-SDK/example.mm')
            item['metadata'].update(CseObjectiveCArc='true', ExceptionHandling='Sync')
            item['overrides'] = dict(item['metadata'])
        lines = []
        translator.emit_platform_sources(lines, 'fixture', configs)
        text = '\n'.join(lines)
        self.assertIn('if(APPLE)', text)
        self.assertIn('OBJCXX_STANDARD 17', text)
        for option in ('-fobjc-arc', '-fobjc-exceptions', '-fobjc-arc-exceptions'):
            self.assertIn(option, text)
        self.assertNotIn('target_compile_options', text)

    def test_objc_arc_rejects_an_unqualified_language_platform_or_value(self):
        for platform, suffix, value in (('macos', '.cpp', 'true'), ('posix', '.mm', 'true'),
                                        ('windows', '.mm', 'true'), ('macos', '.mm', 'yes')):
            with self.subTest(platform=platform, suffix=suffix, value=value):
                configs = self.fixture()
                for config in configs.values():
                    item = config['items'][0]
                    item['path'] = str(ROOT / ('GoldSrc-SDK/example' + suffix))
                    item['metadata'].update(CseTargetOS=platform, CseObjectiveCArc=value)
                    item['overrides'] = dict(item['metadata'])
                with self.assertRaisesRegex(ValueError, 'CseObjectiveCArc'):
                    translator.emit_platform_sources([], 'fixture', configs)

    @unittest.skipUnless(sys.platform == 'darwin', 'Requires the actual Apple Objective-C++ compiler')
    def test_runtime_defaults_do_not_inject_assertion_or_configuration_policy_into_objcxx(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'probe.mm').write_text('#ifdef NDEBUG\n#error Assertion policy leaked from CMake defaults\n'
                                         '#endif\nint objc_policy_probe() { return 0; }\n')
            (root / 'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.25)
set(CMAKE_USER_MAKE_RULES_OVERRIDE "{ROOT}/cmake/MSVCDefaults.cmake")
project(ObjcRuntimeDefaults OBJCXX)
add_library(probe OBJECT probe.mm)
file(WRITE "${{CMAKE_BINARY_DIR}}/flags.txt" "${{CMAKE_OBJCXX_FLAGS_RELEASE}}|${{CMAKE_OBJCXX_FLAGS_DEBUG}}")
''')
            environment = os.environ.copy()
            environment.pop('OBJCXXFLAGS', None)
            for configuration in ('Release', 'Debug'):
                build = root / configuration
                result = subprocess.run(['cmake', '-S', str(root), '-B', str(build), '-G', 'Ninja',
                                         '-DCMAKE_BUILD_TYPE=' + configuration], env=environment,
                                        capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                result = subprocess.run(['cmake', '--build', str(build)], env=environment,
                                        capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual((build / 'flags.txt').read_text(), '|')

    def test_apple_framework_is_an_explicit_target_platform_dependency(self):
        configs = self.fixture()
        for config in configs.values():
            config['items'][0]['metadata']['CseAppleFramework'] = 'Cocoa'
        lines = []
        translator.emit_platform_sources(lines, 'fixture', configs)
        self.assertIn('target_link_libraries(fixture PRIVATE "-framework Cocoa")', '\n'.join(lines))
        for config in configs.values():
            config['items'][0]['metadata']['CseTargetOS'] = 'windows'
        with self.assertRaisesRegex(ValueError, 'CseAppleFramework'):
            translator.emit_platform_sources([], 'fixture', configs)

    def test_unsupported_platform_is_rejected(self):
        configs = self.fixture()
        for config in configs.values():
            config['items'][0]['metadata']['CseTargetOS'] = 'host'
        with self.assertRaisesRegex(ValueError, 'CseTargetOS'):
            translator.emit_platform_sources([], 'fixture', configs)

    def test_platform_membership_must_match_both_configurations(self):
        configs = self.fixture()
        configs['Debug']['items'][0]['metadata']['CseTargetOS'] = 'linux'
        with self.assertRaisesRegex(ValueError, 'configuration'):
            translator.emit_platform_sources([], 'fixture', configs)

    def test_native_only_source_cannot_leak_into_reference_selection(self):
        configs = self.fixture()
        configs['Release']['items'][0]['metadata']['ExcludedFromBuild'] = 'false'
        with self.assertRaisesRegex(ValueError, 'Win32'):
            translator.emit_platform_sources([], 'fixture', configs)

    def test_shared_font_service_has_one_manifest_owned_definition(self):
        configs = self.fixture()
        for config in configs.values():
            config['items'][0]['metadata']['CseNativeServices'] = 'font-registration-owner'
        lines = []
        translator.emit_platform_sources(lines, 'fixture', configs)
        text = '\n'.join(lines)
        self.assertIn('cse_define_mac_font_registration_service("${CSE_SDK_DIR}/example.cpp")', text)
        self.assertNotIn('target_sources(fixture', text)

    def test_native_browser_sources_are_width_conditional(self):
        configs = self.fixture()
        for config in configs.values():
            item = config['items'][0]
            item['metadata'].pop('CseTargetOS')
            item['metadata'].update(CsePointerWidth='64', CseNativeServices='browser', ExceptionHandling='Sync')
            item['overrides'] = dict(item['metadata'])
        lines = []
        translator.emit_platform_sources(lines, 'fixture', configs)
        text = '\n'.join(lines)
        self.assertIn('if(CMAKE_SIZEOF_VOID_P EQUAL 8)', text)
        self.assertIn('target_sources(fixture PRIVATE "${CSE_SDK_DIR}/example.cpp")', text)
        self.assertIn('cse_configure_native_browser(fixture)', text)
        self.assertIn('/EHsc', text)

    def test_coreaudio_service_requires_explicit_macos_ownership(self):
        configs = self.fixture()
        for config in configs.values():
            config['items'][0]['metadata']['CseNativeServices'] = 'voice-mixer'
        lines = []
        translator.emit_platform_sources(lines, 'fixture', configs)
        text = '\n'.join(lines)
        self.assertIn('if(APPLE)', text)
        self.assertIn('cse_link_native_voice_mixer(fixture)', text)
        for config in configs.values():
            config['items'][0]['metadata']['CseTargetOS'] = 'linux'
        with self.assertRaisesRegex(ValueError, 'CseNativeServices'):
            translator.emit_platform_sources([], 'fixture', configs)

    def test_replacement_requires_a_reference_source(self):
        configs = self.fixture()
        for config in configs.values():
            config['items'][0]['metadata']['CseReplaces'] = str(ROOT / 'GoldSrc-SDK/missing.cpp')
        with self.assertRaisesRegex(ValueError, 'CseReplaces'):
            translator.emit_platform_sources([], 'fixture', configs)

    def test_replacement_contract_must_match_configurations(self):
        configs = self.fixture()
        original = str(ROOT / 'GoldSrc-SDK/original.cpp')
        for config in configs.values():
            config['items'].append({'kind': 'ClCompile', 'path': original, 'metadata': {}, 'overrides': {}})
        configs['Release']['items'][0]['metadata']['CseReplaces'] = original
        with self.assertRaisesRegex(ValueError, 'configuration'):
            translator.emit_platform_sources([], 'fixture', configs)

    def test_owned_posix_backend_replaces_only_native_compilation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            original = root / 'original.cpp'
            native = root / 'native.cpp'
            original.write_text('int Backend() { return 32; }\n')
            native.write_text('int Backend() { return 64; }\n')
            (root / 'main.cpp').write_text('int Backend();\nint main() { return Backend() == EXPECTED ? 0 : 1; }\n')
            items = [{'kind': 'ClCompile', 'path': str(original), 'metadata': {}, 'overrides': {}},
                     {'kind': 'ClCompile', 'path': str(native), 'overrides': {},
                      'metadata': {'ExcludedFromBuild': 'true', 'CseTargetOS': 'posix',
                                   'CseReplaces': str(original)}}]
            configs = {config: {'items': deepcopy(items)} for config in ('Debug', 'Release')}
            lines = []
            translator.emit_platform_sources(lines, 'fixture', configs)
            (root / 'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.25)\n'
                'project(OwnedSourceReplacement CXX)\nset(WIN32 ${SIMULATE_WINDOWS})\n'
                'add_executable(fixture main.cpp original.cpp)\n'
                'if(SIMULATE_WINDOWS)\n  target_compile_definitions(fixture PRIVATE EXPECTED=32)\n'
                'else()\n  target_compile_definitions(fixture PRIVATE EXPECTED=64)\nendif()\n'
                + '\n'.join(lines) + '\n')
            for windows in ('ON', 'OFF'):
                for configuration in ('Release', 'Debug'):
                    build = root / (windows + configuration)
                    result = subprocess.run(['cmake', '-S', str(root), '-B', str(build), '-G', 'Ninja',
                        f'-DSIMULATE_WINDOWS={windows}', f'-DCMAKE_BUILD_TYPE={configuration}',
                        '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON'], capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    result = subprocess.run(['cmake', '--build', str(build)], capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    result = subprocess.run([str(build / 'fixture')], capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    files = [Path(item['file']).name for item in json.loads((build / 'compile_commands.json').read_text())]
                    self.assertEqual(files.count('original.cpp'), int(windows == 'ON'))
                    self.assertEqual(files.count('native.cpp'), int(windows == 'OFF'))

    def test_unsupported_pointer_width_is_rejected(self):
        configs = self.fixture()
        for config in configs.values():
            config['items'][0]['metadata']['CsePointerWidth'] = '128'
        with self.assertRaisesRegex(ValueError, 'CsePointerWidth'):
            translator.emit_platform_sources([], 'fixture', configs)


if __name__ == '__main__':
    unittest.main()
