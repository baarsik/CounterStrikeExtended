from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from portable_game_policy import settings


class PortableGamePolicyTests(unittest.TestCase):
    def test_preserves_assertion_definitions_without_injecting_ndebug(self):
        result = settings({'PreprocessorDefinitions': 'WIN32;_WINDOWS;VALVE_DLL;_DEBUG;UTL_ENGINE_ASSERTS',
                           'Optimization': 'MaxSpeed'})
        self.assertEqual(result['definitions'], ['VALVE_DLL', '_DEBUG', 'UTL_ENGINE_ASSERTS'])
        self.assertIn('-O2', result['common'])
        self.assertNotIn('NDEBUG', result['definitions'])

    def test_windows_decoder_and_socket_contracts_do_not_leak_to_posix(self):
        result = settings({'PreprocessorDefinitions':
                           'LINK_MPG123_DLL;CSE_ENGINE_WSOCK32=1;UTL_ENGINE_ASSERTS;HL25_HW_ENGINE'})
        self.assertEqual(result['definitions'], ['UTL_ENGINE_ASSERTS', 'HL25_HW_ENGINE'])

    def test_preserves_default_and_explicit_exception_policies(self):
        self.assertIn('-fno-exceptions', settings({})['cxx'])
        self.assertIn('-fexceptions', settings({'ExceptionHandling': 'Sync'})['cxx'])
        self.assertNotIn('-fexceptions', settings({'ExceptionHandling': 'Sync'})['common'])

    def test_cpp_frames_allow_outer_unwinding_without_enabling_inner_handlers(self):
        for exception_policy in ('', 'false', 'Sync', 'Async'):
            with self.subTest(exception_policy=exception_policy):
                result = settings({'ExceptionHandling': exception_policy,
                                   'CsePortableFaultPolicy': 'OsTermination'})
                self.assertIn('-funwind-tables', result['cxx'])
                self.assertNotIn('-funwind-tables', result['common'])
                expected = '-fexceptions' if exception_policy in ('Sync', 'Async') else '-fno-exceptions'
                self.assertEqual(result['cxx'][0], expected)

    def test_async_requires_explicit_portable_fault_policy(self):
        with self.assertRaises(ValueError):
            settings({'ExceptionHandling': 'Async'})
        result = settings({'ExceptionHandling': 'Async',
                           'CsePortableFaultPolicy': 'OsTermination'})
        self.assertIn('-fexceptions', result['cxx'])
        self.assertEqual(result['hardware_faults'], 'OsTermination')
        with self.assertRaisesRegex(ValueError, 'fault policy'):
            settings({'ExceptionHandling': 'Async', 'CsePortableFaultPolicy': 'TranslateSeh'})
        self.assertEqual(settings({'ExceptionHandling': 'Sync'})['hardware_faults'], 'OsTermination')

    def test_preserves_explicit_floating_point_models(self):
        precise = settings({'FloatingPointModel': 'Precise'})['common']
        self.assertIn('-ffp-model=precise', precise)
        self.assertIn('-ffp-contract=off', precise)
        self.assertIn('-ffast-math', settings({'FloatingPointModel': 'Fast'})['common'])
        self.assertIn('-ffp-model=strict', settings({'FloatingPointModel': 'Strict'})['common'])
        with self.assertRaisesRegex(ValueError, 'floating'):
            settings({'FloatingPointModel': 'Unknown'})

    def test_rejects_untranslated_semantic_options(self):
        with self.assertRaisesRegex(ValueError, 'AdditionalOptions'):
            settings({'AdditionalOptions': '/fp:fast /unknown'})
        self.assertEqual(settings({'AdditionalOptions': '/std:c++17 '})['standard'], 17)

    def test_keeps_new_diagnostics_fatal_without_erasing_legacy_diagnostics(self):
        options = settings({'TreatWarningAsError': 'true'})['common']
        self.assertIn('-Werror', options)
        self.assertIn('-Wno-error=writable-strings', options)
        self.assertIn('-Wno-error=invalid-offsetof', options)
        self.assertNotIn('-Wno-error', options)
        self.assertNotIn('-w', options)
        self.assertIn('-Werror=shorten-64-to-32', options)
        self.assertIn('-Werror=pointer-to-int-cast', options)
        self.assertNotIn('-Wno-error=pointer-to-int-cast', options)
        self.assertNotIn('-Wno-error=shorten-64-to-32', options)

    def test_keeps_symbol_generation_explicit(self):
        self.assertFalse(settings({'DebugInformationFormat': 'None'})['symbols'])
        self.assertTrue(settings({'DebugInformationFormat': 'ProgramDatabase'})['symbols'])
        with self.assertRaisesRegex(ValueError, 'debug information'):
            settings({'DebugInformationFormat': 'Unknown'})

    def test_per_file_msvc_symbol_options_translate_without_changing_optimization(self):
        for option in ('/Z7', '/Zi'):
            result = settings({'AdditionalOptions': option})
            self.assertTrue(result['symbols'])
            self.assertIn('-O0', result['common'])

    def test_keeps_rtti_and_optimization_choices_explicit(self):
        self.assertIn('-frtti', settings({'RuntimeTypeInfo': 'true'})['cxx'])
        self.assertIn('-fno-rtti', settings({'RuntimeTypeInfo': 'false'})['cxx'])
        self.assertIn('-O0', settings({'Optimization': 'Disabled'})['common'])
        with self.assertRaisesRegex(ValueError, 'Optimization'):
            settings({'Optimization': 'Unknown'})


if __name__ == '__main__':
    unittest.main()
