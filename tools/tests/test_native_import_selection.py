from pathlib import Path
import subprocess
import tempfile
import unittest
from test_binary_identity import archive, coff

ROOT = Path(__file__).resolve().parents[2]


class NativeImportSelectionTests(unittest.TestCase):
    def configure(self, root, properties=''):
        prefix = root / 'pinned'
        for configuration in ['lib', 'debug/lib']:
            path = prefix / configuration
            path.mkdir(parents=True)
            (path / 'provider.lib').write_bytes(archive(('provider.obj/', coff(0x8664))))
        (prefix / 'include').mkdir()
        (root / 'foreign-headers').mkdir()
        (root / 'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.25)
project(ImportSelection NONE)
set(CSE_TARGET_ARCH x64)
include("{ROOT}/cmake/NativeThirdParty.cmake")
add_library(provider STATIC IMPORTED)
set_target_properties(provider PROPERTIES
  IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
  IMPORTED_LOCATION_DEBUG "{prefix}/debug/lib/provider.lib"
  IMPORTED_LOCATION_RELEASE "{prefix}/lib/provider.lib"
  INTERFACE_INCLUDE_DIRECTORIES "{prefix}/include")
{properties}
cse_validate_native_import(provider "{prefix}")
file(GENERATE OUTPUT "${{CMAKE_BINARY_DIR}}/selected.txt" CONTENT "$<TARGET_FILE:provider>")
''')
        return subprocess.run(['cmake', '-G', 'Ninja', '-S', str(root), '-B', str(root / 'build'),
                               '-DCMAKE_BUILD_TYPE=Debug'], capture_output=True, text=True)

    def test_debug_uses_the_pinned_debug_archive(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            result = self.configure(root)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual((root / 'build/selected.txt').read_text(),
                             (root / 'pinned/debug/lib/provider.lib').as_posix())

    def test_release_mapping_or_missing_debug_location_cannot_fall_back(self):
        for property in ['MAP_IMPORTED_CONFIG_DEBUG RELEASE', 'IMPORTED_LOCATION_DEBUG ""']:
            with self.subTest(property=property), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                result = self.configure(root, 'set_target_properties(provider PROPERTIES ' + property + ')')
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertRegex(result.stdout + result.stderr, 'configuration|DEBUG|Debug')

    def test_cached_foreign_header_directory_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            properties = f'set_target_properties(provider PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "{root}/foreign-headers")'
            result = self.configure(root, properties)
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('header', (result.stdout + result.stderr).lower())


if __name__ == '__main__':
    unittest.main()
