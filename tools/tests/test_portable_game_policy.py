"""CMake's portable game recipes preserve semantic compiler and assertion policy."""
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from cmake_inventory import query_target


class PortableGamePolicyTests(unittest.TestCase):
    def test_half_life_keeps_assertions_and_explicit_exceptions_rtti_and_floating_point(self):
        for configuration in ('Debug', 'Release'):
            with self.subTest(configuration=configuration):
                data = query_target(ROOT, 'hl_server', configuration, 'macos', 'arm64')
                for source in data['sources']:
                    if source['language'] != 'CXX':
                        continue
                    flags = source['compile_options']
                    definitions = source['compile_definitions']
                    self.assertNotIn('UTL_ENGINE_ASSERTS', definitions)
                    self.assertNotIn('WIN32', definitions)
                    self.assertNotIn('_WINDOWS', definitions)
                    self.assertEqual('NDEBUG' in definitions,
                                     configuration == 'Release')
                    exceptions = [flag for flag in flags if flag in ('-fexceptions', '-fno-exceptions')]
                    self.assertTrue(exceptions)
                    if Path(source['path']).name == 'saved_callbacks.cpp':
                        self.assertEqual(exceptions[-1], '-fexceptions')
                    else:
                        self.assertEqual(exceptions[-1], '-fno-exceptions')
                    self.assertIn('-frtti', flags)
                    self.assertIn('-ffp-model=precise', flags)
                    self.assertIn('-ffp-contract=off', flags)
                    self.assertIn('-funwind-tables', flags)
                    self.assertIn('-O0' if configuration == 'Debug' else '-O2', flags)
                    self.assertIn('-Werror=shorten-64-to-32', flags)
                    self.assertIn('-Werror=pointer-to-int-cast', flags)
                    self.assertIn('-Wno-error=writable-strings', flags)
                    self.assertNotIn('-w', flags)
                    self.assertNotIn('-Wno-error', flags)
                    self.assertEqual('_DEBUG' in definitions, configuration == 'Debug')
                    self.assertIn(str(ROOT / 'src/public/cse_posix_crt.h'), flags)

    def test_symbol_generation_requires_debug_or_release_opt_in(self):
        for configuration, diagnostics, expected in (('Release', False, False),
                                                       ('Release', True, True), ('Debug', False, True)):
            data = query_target(ROOT, 'cs_server', configuration, 'macos', 'arm64', diagnostics)
            for source in data['sources']:
                if source['language'] == 'CXX':
                    self.assertEqual('-g' in source['compile_options'], expected)

    def test_windows_keeps_crt_and_compiler_policies_for_each_configuration(self):
        for configuration in ('Debug', 'Release'):
            data = query_target(ROOT, 'cs_server', configuration)
            self.assertEqual(data['properties']['MSVC_RUNTIME_LIBRARY'],
                             'MultiThreadedDebugDLL' if configuration == 'Debug' else 'MultiThreadedDLL')
            for source in data['sources']:
                if source['language'] != 'CXX':
                    continue
                self.assertIn('/EHsc', source['compile_options'])
                self.assertIn('/GR', source['compile_options'])
                self.assertIn('/Od' if configuration == 'Debug' else '/O2', source['compile_options'])
                self.assertIn('REGAMEDLL_FIXES', source['compile_definitions'])
                self.assertIn('CSE_SHARED_GAME_CONTRACT', source['compile_definitions'])


if __name__ == '__main__':
    unittest.main()
