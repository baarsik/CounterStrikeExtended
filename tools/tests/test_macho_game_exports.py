import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from macho_game_exports import check_exports, read_reference


class MachoGameExportTests(unittest.TestCase):
    def test_reads_actual_dumpbin_row_shape_and_rejects_empty_reports(self):
        reference = read_reference('    ordinal hint RVA      name\n'
                                   '          1    0 00006080 CseGetModuleAbi\n'
                                   '         19   12 0000A020 ?SpinUpCall@CBaseTurret@@UAEXXZ\n')
        self.assertEqual(reference, ['CseGetModuleAbi', '?SpinUpCall@CBaseTurret@@UAEXXZ'])
        with self.assertRaisesRegex(ValueError, 'No reference'):
            read_reference('No exports here\n')

    def test_requires_c_entrypoints_and_every_overload(self):
        reference = ['CseGetModuleAbi', '?Use@CButton@@QAEXH@Z', '?Use@CButton@@QAEXXZ']
        result = check_exports(reference, ['_CseGetModuleAbi'], ['CButton::Use(int)', 'CButton::Use()'])
        self.assertEqual(result, {'retained_c': 1, 'retained_cpp': 2})
        with self.assertRaisesRegex(ValueError, 'Use@CButton'):
            check_exports(reference, ['_CseGetModuleAbi'], ['CButton::Use(int)'])
        with self.assertRaisesRegex(ValueError, 'CseGetModuleAbi'):
            check_exports(reference, [], ['CButton::Use(int)', 'CButton::Use()'])

    def test_inline_base_export_cannot_be_replaced_by_a_derived_override(self):
        reference = ['?SpinDownCall@CBaseTurret@@UAEXXZ', '?SpinUpCall@CBaseTurret@@UAEXXZ']
        with self.assertRaisesRegex(ValueError, 'SpinDownCall@CBaseTurret'):
            check_exports(reference, [], ['CTurret::SpinDownCall()', 'CTurret::SpinUpCall()'])
        result = check_exports(reference, [], ['CBaseTurret::SpinDownCall()', 'CBaseTurret::SpinUpCall()'])
        self.assertEqual(result['retained_cpp'], 2)

    def test_extra_exports_do_not_conceal_a_missing_method(self):
        with self.assertRaisesRegex(ValueError, 'Restore@CWorld'):
            check_exports(['?Restore@CWorld@@UAEHAAVCRestore@@@Z'], [],
                          ['CWorld::Save(CSave&)', 'COther::Restore(CRestore&)'])


if __name__ == '__main__':
    unittest.main()
