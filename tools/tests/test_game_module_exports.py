import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from game_module_exports import native_definition


class GameModuleExportTests(unittest.TestCase):
    def test_native_dedicated_and_particle_definitions_keep_factory_ordinals(self):
        root = Path(__file__).resolve().parents[2]
        for path, expected in [('GoldSrc-SDK/swds/swds.def', ['CreateInterface @367', 'F @368']),
                               ('GoldSrc-SDK/projects/vs2026/particleman.def', ['CreateInterface @2'])]:
            with self.subTest(path=path):
                result = native_definition((root / path).read_text())
                exports = result.split('EXPORTS\n', 1)[1].strip().splitlines()
                self.assertEqual([line.strip() for line in exports], expected)

    def test_native_cs_client_keeps_c_entry_ordinals(self):
        root = Path(__file__).resolve().parents[2]
        result = native_definition((root / 'GoldSrc-SDK/projects/vs2026/cstrike_cdll.def').read_text())
        self.assertNotIn('?AttemptToMaterialize', result)
        for entry in ['ClientFactory @18', 'CreateInterface @19', 'F @21']:
            self.assertIn(entry, result)

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

    def test_native_a3d_keeps_private_com_exports_and_ordinals(self):
        root = Path(__file__).resolve().parents[2]
        result = native_definition((root / 'GoldSrc-SDK/a3dapi/a3dapi.def').read_text())
        self.assertEqual(result, 'LIBRARY A3dApi\nEXPORTS\n'
                         '\tDllGetClassObject @2 PRIVATE\n\tDllCanUnloadNow @3 PRIVATE\n')

    def test_rejects_unknown_sections_and_malformed_exports(self):
        for line in ('entry @-1', 'entry unexpected', 'IMPORTS', '.text EXECUTE'):
            with self.subTest(line=line), self.assertRaises(ValueError):
                native_definition('LIBRARY owned\nEXPORTS\n' + line + '\n')

    def test_rejects_missing_export_block(self):
        with self.assertRaises(ValueError):
            native_definition('LIBRARY owned\n')


if __name__ == '__main__':
    unittest.main()
