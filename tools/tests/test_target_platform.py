"""Compiler target detection must ignore the host and reject requested mismatches."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class TargetPlatformTests(unittest.TestCase):
    def test_macos_defaults_apply_to_target_before_project(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            script = root / 'defaults.cmake'
            script.write_text(f'''include("{ROOT}/cmake/PlatformDefaults.cmake")
file(WRITE "${{RESULT}}" "${{CMAKE_OSX_DEPLOYMENT_TARGET}}")
''')
            for target, version, expected in [('Darwin', None, '14.0'), ('Darwin', '14.5', '14.5'),
                                              ('Windows', None, '')]:
                output = root / 'result.txt'
                command = ['cmake', '-DCMAKE_SYSTEM_NAME=' + target, '-DRESULT=' + str(output)]
                if version:
                    command.append('-DCMAKE_OSX_DEPLOYMENT_TARGET=' + version)
                result = subprocess.run(command + ['-P', str(script)], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual(output.read_text(), expected)
            result = subprocess.run(['cmake', '-DCMAKE_SYSTEM_NAME=Darwin', '-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0',
                                     '-P', str(script)], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('14', result.stdout + result.stderr)

    def test_compiler_target_is_detected_and_checked(self):
        module = ROOT / 'cmake/TargetPlatform.cmake'
        self.assertTrue(module.exists(), 'compiler-target detection is required')
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.25)
project(TargetIdentity CXX)
set(CMAKE_HOST_SYSTEM_PROCESSOR intentionally-incorrect-host)
include("{module}")
file(WRITE "${{CMAKE_BINARY_DIR}}/target.txt" "${{CSE_TARGET_OS}};${{CSE_TARGET_ARCH}};${{CSE_COMPILER_ABI}};${{CMAKE_SIZEOF_VOID_P}}")
''')
            command = ['cmake', '-S', str(root), '-G', 'Ninja']
            result = subprocess.run(command + ['-B', str(root / 'valid')], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            target_os, architecture, abi, width = (root / 'valid/target.txt').read_text().split(';')
            self.assertIn(target_os, ['windows', 'macos', 'linux'])
            self.assertIn(architecture, ['x86', 'x64', 'arm64'])
            self.assertIn(abi, ['msvc', 'itanium'])
            self.assertEqual(width, '4' if architecture == 'x86' else '8')
            wrong = 'x86' if architecture != 'x86' else 'arm64'
            result = subprocess.run(command + ['-B', str(root / 'invalid'), '-DCSE_TARGET_ARCH=' + wrong],
                                    capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('compiler target', result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
