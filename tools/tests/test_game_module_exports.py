import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from game_module_exports import native_definition


class GameModuleExportTests(unittest.TestCase):
    def test_retains_c_exports_ordinals_and_sections(self):
        source = ('LIBRARY owned\nEXPORTS\n    CseGetModuleAbi\n'
                  '    ?Think@Owned@@QAEXXZ @3\n    worldspawn @359\n'
                  'SECTIONS\n    .data READ WRITE\n')
        self.assertEqual(native_definition(source),
                         'LIBRARY owned\nEXPORTS\n    CseGetModuleAbi\n    worldspawn @359\n'
                         'SECTIONS\n    .data READ WRITE\n')

    def test_rejects_aliases_that_could_bind_to_a_stale_x86_symbol(self):
        with self.assertRaisesRegex(ValueError, 'Unsupported'):
            native_definition('LIBRARY owned\nEXPORTS\nThink = ?Think@Owned@@QAEXXZ\n')

    def test_translates_a_c_stdcall_self_alias_without_changing_its_ordinal(self):
        source = 'LIBRARY owned\nEXPORTS\n    GiveFnptrsToDll=_GiveFnptrsToDll@8 @1\n'
        self.assertEqual(native_definition(source), 'LIBRARY owned\nEXPORTS\n    GiveFnptrsToDll @1\n')

    def test_rejects_unknown_sections_and_malformed_exports(self):
        for line in ('entry @-1', 'entry unexpected', 'IMPORTS', '.text EXECUTE'):
            with self.subTest(line=line), self.assertRaises(ValueError):
                native_definition('LIBRARY owned\nEXPORTS\n' + line + '\n')

    def test_rejects_missing_export_block(self):
        with self.assertRaises(ValueError):
            native_definition('LIBRARY owned\n')


if __name__ == '__main__':
    unittest.main()
