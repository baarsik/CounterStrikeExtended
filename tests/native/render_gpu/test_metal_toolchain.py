"""Filesystem-only regressions for the reviewed toolchain alias provenance gap."""
import hashlib
import os
from pathlib import Path
import tempfile
import unittest

from compile_metal import verify_toolchain


@unittest.skipIf(os.name == 'nt', 'Native Metal alias profile uses POSIX symlinks')
class MetalAliasTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.target = self.root / 'approved'
        self.target.write_bytes(b'pinned tool')
        self.other = self.root / 'other'
        self.other.write_bytes(b'other tool')
        self.alias = self.root / 'launcher'
        self.alias.symlink_to('approved')
        self.pins = {'files': [{'path': 'approved', 'bytes': 11,
                               'sha256': hashlib.sha256(self.target.read_bytes()).hexdigest()}],
                     'symlinks': [{'path': 'launcher', 'target': 'approved', 'resolved': 'approved'}]}

    def test_approved_alias(self):
        verify_toolchain(self.root, self.pins)

    def test_retargeted_alias_is_rejected(self):
        self.alias.unlink()
        self.alias.symlink_to('other')
        with self.assertRaises(ValueError):
            verify_toolchain(self.root, self.pins)

    def test_replaced_alias_is_rejected(self):
        self.alias.unlink()
        self.alias.write_bytes(self.target.read_bytes())
        with self.assertRaises(ValueError):
            verify_toolchain(self.root, self.pins)

    def test_windows_catalog_without_aliases_still_verifies(self):
        del self.pins['symlinks']
        verify_toolchain(self.root, self.pins)


if __name__ == '__main__':
    unittest.main()
