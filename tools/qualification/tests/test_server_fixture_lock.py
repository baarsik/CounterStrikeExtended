"""Reject changed, foreign-architecture and unsafe external-server packages."""
import hashlib
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest
import zipfile
from test_audit_sdl3 import pe

MODULE = Path(__file__).parents[1] / 'verify_server_fixture.py'
if MODULE.exists():
    spec = importlib.util.spec_from_file_location('verify_server_fixture', MODULE)
    verifier = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(verifier)
else:
    verifier = None


class ServerFixtureLockTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(verifier, 'server fixture verifier is required')
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def package(self, data, name='bin/win32/swds.dll'):
        path = self.root / 'server.zip'
        with zipfile.ZipFile(path, 'w') as archive:
            archive.writestr(name, data)
        return {'schema': 1, 'architecture': 'x86', 'packages': [
            {'archive': path.name, 'version': 'fixture', 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
             'native_files': {name: hashlib.sha256(data).hexdigest()}}]}

    def test_pinned_package_is_static_evidence_only(self):
        report = verifier.verify_packages(self.package(pe()), self.root)
        self.assertFalse(report['qualified'])
        self.assertEqual(report['evidence'], 'pinned-external-server-binaries')
        self.assertEqual(report['packages']['server.zip']['native_files'], ['bin/win32/swds.dll'])

    def test_changed_archive_or_native_member_is_rejected(self):
        lock = self.package(pe())
        (self.root / 'server.zip').write_bytes(b'changed archive')
        with self.assertRaisesRegex(ValueError, 'hash'):
            verifier.verify_packages(lock, self.root)
        lock = self.package(pe())
        lock['packages'][0]['native_files']['bin/win32/swds.dll'] = '0' * 64
        with self.assertRaisesRegex(ValueError, 'hash'):
            verifier.verify_packages(lock, self.root)

    def test_foreign_machine_is_rejected_even_with_correct_hashes(self):
        data = bytearray(pe())
        struct.pack_into('<H', data, 132, 0xaa64)
        lock = self.package(data)
        with self.assertRaisesRegex(ValueError, 'width|architecture'):
            verifier.verify_packages(lock, self.root)

    def test_unsafe_archive_and_native_paths_are_rejected(self):
        for name in ['../swds.dll', '/swds.dll', 'C:/swds.dll', 'bin/../../swds.dll']:
            with self.subTest(name=name), self.assertRaises(ValueError):
                verifier.verify_packages(self.package(pe(), name), self.root)


if __name__ == '__main__':
    unittest.main()
