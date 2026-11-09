"""Regression cases for compiler-observed Ninja dependency path handling."""
from pathlib import Path
import tempfile
import unittest

from verify_build_inputs import ninja_dependency_paths


class NinjaDependencyPathsTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.build = self.root / 'build' / 'Release'
        self.build.mkdir(parents=True)
        self.native = self.root / 'native packages' / 'include'

    def header(self, filename, root=None):
        path = (self.native if root is None else root) / filename
        path.parent.mkdir(parents=True, exist_ok=True)
        path.touch()
        return path.resolve()

    def test_resolves_relative_forward_backward_and_absolute_records(self):
        freetype = self.header('freetype/config/ftconfig.h')
        zconf = self.header('zconf.h')
        webm = self.header('webm/mkvparser/mkvparser.h')
        receipt = ('CMakeFiles/probe.obj: #deps 3, deps mtime 123 (VALID)\r\n'
                   '    ../../native packages/include/freetype/config/ftconfig.h\r\n'
                   '    ..\\..\\native packages\\include\\zconf.h\r\n'
                   '    ' + str(webm) + '\r\n\r\n')
        self.assertEqual(ninja_dependency_paths(receipt, self.build), {freetype, zconf, webm})

    def test_ignores_object_metadata_and_deduplicates_header_records(self):
        zconf = self.header('zconf.h')
        receipt = ('CMakeFiles/probe.obj: #deps 1, deps mtime 123 (VALID)\n'
                   '    ../../native packages/include/zconf.h\n\n'
                   'CMakeFiles/second.obj: #deps 1, deps mtime 124 (VALID)\n'
                   '    ../../native packages/include/zconf.h\n')
        self.assertEqual(ninja_dependency_paths(receipt, self.build), {zconf})

    def test_foreign_same_named_header_cannot_satisfy_native_identity(self):
        native = self.header('freetype/config/ftconfig.h')
        foreign = self.header('freetype/config/ftconfig.h', self.root / 'foreign' / 'include')
        paths = ninja_dependency_paths('    ../../foreign/include/freetype/config/ftconfig.h\n', self.build)
        self.assertEqual(paths, {foreign})
        self.assertNotIn(native, paths)

    def test_symlink_receipt_exposes_retained_x86_header_identity(self):
        poison = self.header('zconf.h', self.root / 'x86-windows-static-md' / 'include')
        try:
            (self.build / 'innocent-header.h').symlink_to(poison)
        except OSError as error:
            self.skipTest('Creating symlinks is unavailable: ' + str(error))
        paths = ninja_dependency_paths('    innocent-header.h\n', self.build)
        self.assertEqual(paths, {poison})
        self.assertTrue(any('x86-windows-static-md' in str(path) for path in paths))


if __name__ == '__main__':
    unittest.main()
