"""Check the generated runtime contract without executing any game modules."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from test_audit_sdl3 import pe

MODULE = Path(__file__).parents[1] / 'audit_runtime_inventory.py'
if MODULE.exists():
    spec = importlib.util.spec_from_file_location('audit_runtime_inventory', MODULE)
    inventory = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(inventory)
else:
    inventory = None


class RuntimeInventoryTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(inventory, 'runtime inventory audit implementation is required')
        self.records = [{'target': 'cs_client', 'runtime': {'kind': 'SHARED', 'name': 'client',
                                                          'directory': 'cstrike/cl_dlls'}}]
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.module = self.root / 'cstrike/cl_dlls/client.dll'
        self.module.parent.mkdir(parents=True)
        self.module.write_bytes(pe('KERNEL32.dll'))

    def test_reports_hashes_and_static_evidence_without_runtime_claim(self):
        report = inventory.audit_inventory(self.root, self.records, 'x86')
        self.assertEqual(report['production_targets'], {'cs_client': 'cstrike/cl_dlls/client.dll'})
        module = report['binaries']['cstrike/cl_dlls/client.dll']
        self.assertEqual(len(module['sha256']), 64)
        self.assertIn('exports', module)
        self.assertEqual(report['evidence'], 'static-binary-inspection')
        self.assertFalse(report['qualified'])

    def test_missing_output_wrong_architecture_and_nonruntime_products_fail(self):
        self.module.unlink()
        with self.assertRaisesRegex(ValueError, 'missing'):
            inventory.audit_inventory(self.root, self.records, 'x86')
        self.module.write_bytes(pe(machine=0xaa64))
        with self.assertRaisesRegex(ValueError, 'width|architecture'):
            inventory.audit_inventory(self.root, self.records, 'x86')
        self.module.write_bytes(pe())
        (self.root / 'client.pdb').touch()
        with self.assertRaisesRegex(ValueError, 'Non-runtime'):
            inventory.audit_inventory(self.root, self.records, 'x86')

    def test_every_extra_provider_is_inspected(self):
        (self.root / 'provider.dll').write_bytes(b'not a PE')
        with self.assertRaises(ValueError):
            inventory.audit_inventory(self.root, self.records, 'x86')

    def test_native_provider_content_cannot_hide_behind_another_extension(self):
        import struct
        foreign = struct.pack('<IIIIIIII', 0xfeedfacf, 0x100000c, 0, 6, 0, 0, 0, 0)
        (self.root / 'foreign.drv').write_bytes(foreign)
        with self.assertRaisesRegex(ValueError, 'architecture|PE'):
            inventory.audit_inventory(self.root, self.records, 'x86')

    def test_invalid_output_contract_cannot_escape_root_or_alias_another_target(self):
        for directory in ['..', '/absolute', 'C:/absolute', 'cstrike/../../outside']:
            self.records[0]['runtime']['directory'] = directory
            with self.subTest(directory=directory), self.assertRaises(ValueError):
                inventory.audit_inventory(self.root, self.records, 'x86')
        self.records[0]['runtime']['directory'] = 'cstrike/cl_dlls'
        self.records.append(dict(self.records[0], target='other'))
        with self.assertRaisesRegex(ValueError, 'Duplicate'):
            inventory.audit_inventory(self.root, self.records, 'x86')


if __name__ == '__main__':
    unittest.main()
