"""Production CMake source membership follows target platform and pointer width."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from cmake_inventory import query_target


def compiled_sources(data):
    return {item['path']: item for item in data['sources'] if item['language']}


class PlatformSourceMembershipTests(unittest.TestCase):
    def test_native_browser_and_base_replacement_follow_pointer_width(self):
        for configuration in ('Debug', 'Release'):
            for target_os, architecture, native in (('windows', 'x86', False),
                                                     ('windows', 'x64', True),
                                                     ('macos', 'arm64', True),
                                                     ('linux', 'x64', True)):
                for target in ('hw_engine', 'swds', 'gameui'):
                    with self.subTest(target=target, configuration=configuration,
                                      target_os=target_os, architecture=architecture):
                        data = query_target(ROOT, target, configuration, target_os, architecture)
                        selected = compiled_sources(data)
                        base = str(ROOT / 'src/common/native/BaseSystemModule.cpp')
                        self.assertEqual(base in selected, native)
                        bases = [item for path, item in selected.items() if path.endswith('/BaseSystemModule.cpp')]
                        self.assertEqual(len(bases), 1)
                        browser = 'GameUI/native/ServerBrowserModel.cpp' if target == 'gameui' else (
                            'engine/html/native/browser_controller.cpp')
                        self.assertEqual(str(ROOT / 'src' / browser) in selected, native)
                        if native:
                            options = selected[str(ROOT / 'src' / browser)]['compile_options']
                            self.assertIn('/EHsc' if target_os == 'windows' else '-fexceptions', options)

    def test_macos_mouse_and_voice_backends_do_not_leak_to_other_platforms(self):
        for target in ('hw_engine', 'swds'):
            for configuration in ('Debug', 'Release'):
                for target_os, architecture in (('windows', 'x86'), ('windows', 'x64'),
                                                ('macos', 'arm64'), ('linux', 'x64')):
                    with self.subTest(target=target, configuration=configuration, target_os=target_os):
                        data = query_target(ROOT, target, configuration, target_os, architecture)
                        sources = compiled_sources(data)
                        self.assertEqual(str(ROOT / 'src/engine/native_mouse_cocoa.cpp') in sources,
                                         target_os == 'macos')
                        self.assertEqual(str(ROOT / 'src/engine/voice_mixer_controls_coreaudio.cpp') in sources,
                                         target_os == 'macos')
                        self.assertEqual(str(ROOT / 'src/engine/native_mouse_windows.cpp') in sources,
                                         target_os == 'windows' and architecture != 'x86')

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


if __name__ == '__main__':
    unittest.main()
