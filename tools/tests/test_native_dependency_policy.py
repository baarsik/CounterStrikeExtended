"""Inspect real provider recipes under the runtime's cleared configuration defaults."""
from pathlib import Path
import json
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class NativeDependencyPolicyTests(unittest.TestCase):
    def test_provider_optimization_is_explicit_and_does_not_reach_the_owner(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / 'source'
            source.mkdir()
            (source / 'owner.c').write_text('int owner_probe(void) { return 0; }\n')
            (source / 'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.25)
set(CMAKE_USER_MAKE_RULES_OVERRIDE "{ROOT}/cmake/MSVCDefaults.cmake")
project(DependencyPolicyProbe C CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CSE_ROOT "{ROOT}")
set(CSE_SDK_DIR "{ROOT}/src")
set(CSE_OUTPUT_DIRECTORY "${{CMAKE_BINARY_DIR}}/runtime")
include("{ROOT}/cmake/TargetPlatform.cmake")
add_subdirectory("{ROOT}/cmake/dependencies/SDL3" sdl)
add_subdirectory("{ROOT}/cmake/dependencies/mpg123" mpg)
add_library(owner_probe OBJECT owner.c)
file(WRITE "${{CMAKE_BINARY_DIR}}/msvc-policy.txt" "${{MSVC}}")
''')
            for configuration in ('Release', 'Debug'):
                with self.subTest(configuration=configuration):
                    build = Path(temporary) / configuration
                    result = subprocess.run(['cmake', '-S', str(source), '-B', str(build), '-G', 'Ninja',
                                             '-DCMAKE_BUILD_TYPE=' + configuration],
                                            capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout[-3000:] + result.stderr)
                    is_msvc = (build / 'msvc-policy.txt').read_text().lower() in ('1', 'on', 'true')
                    database = json.loads((build / 'compile_commands.json').read_text())
                    providers = [entry for entry in database if Path(entry['file']).suffix.lower() in ('.c', '.cpp', '.m', '.mm')
                                 and any(marker in entry['output'].replace('\\', '/')
                                 for marker in ('/SDL3-shared.dir/', '/libmpg123.dir/', '/compat.dir/'))]
                    self.assertTrue(providers)
                    seen = set()
                    for entry in providers:
                        arguments = shlex.split(entry['command'], posix=True)
                        options = [item for item in arguments if item.startswith(('-O', '/O'))]
                        expected = ('/O2' if configuration == 'Release' else '/Od') if is_msvc else (
                            '-O2' if configuration == 'Release' else '-O0')
                        self.assertTrue(options, (configuration, entry['file'], 'missing optimization policy'))
                        self.assertEqual(options[-1], expected, (configuration, entry['file'], options))
                        if configuration == 'Debug':
                            self.assertIn('/Zi' if is_msvc else '-g', arguments, entry['file'])
                        seen.update(marker for marker in ('SDL3-shared.dir', 'libmpg123.dir', 'compat.dir')
                                    if marker in entry['output'])
                    self.assertEqual(seen, {'SDL3-shared.dir', 'libmpg123.dir', 'compat.dir'})
                    owner = next(entry for entry in database if entry['file'].endswith('/owner.c'))
                    self.assertFalse(any(item.startswith(('-O', '/O', '-DNDEBUG', '/DNDEBUG'))
                                         for item in shlex.split(owner['command'])))


if __name__ == '__main__':
    unittest.main()
