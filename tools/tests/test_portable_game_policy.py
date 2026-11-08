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

    def test_preserves_default_and_explicit_exception_policies(self):
        self.assertIn('-fno-exceptions', settings({})['cxx'])
        self.assertIn('-fexceptions', settings({'ExceptionHandling': 'Sync'})['cxx'])
        self.assertNotIn('-fexceptions', settings({'ExceptionHandling': 'Sync'})['common'])

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

    def test_keeps_rtti_and_optimization_choices_explicit(self):
        self.assertIn('-frtti', settings({'RuntimeTypeInfo': 'true'})['cxx'])
        self.assertIn('-fno-rtti', settings({'RuntimeTypeInfo': 'false'})['cxx'])
        self.assertIn('-O0', settings({'Optimization': 'Disabled'})['common'])
        with self.assertRaisesRegex(ValueError, 'Optimization'):
            settings({'Optimization': 'Unknown'})


if __name__ == '__main__':
    unittest.main()
