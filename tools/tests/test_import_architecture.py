"""CMake must validate actual archive members, independently of path spelling."""
from pathlib import Path
import subprocess
import tempfile
import unittest
from test_binary_identity import archive, coff

ROOT = Path(__file__).resolve().parents[2]


class ImportArchitectureTests(unittest.TestCase):
    def configure(self, root, architecture, payload, folder):
        source = root / 'source'
        source.mkdir()
        library = root / folder / 'provider.lib'
        library.parent.mkdir()
        library.write_bytes(payload)
        (source / 'stub.c').write_text('int probe(void) { return 0; }\n')
        (source / 'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.25)
project(ImportIdentity C)
set(CSE_ROOT "{ROOT}")
set(CSE_TARGET_ARCH "{architecture}")
include("{ROOT}/cmake/RuntimeTargets.cmake")
add_library(consumer STATIC stub.c)
cse_link_import(consumer Release "{library}")
''')
        return subprocess.run(['cmake', '-G', 'Ninja', '-S', str(source), '-B', str(root / 'build')],
                              capture_output=True, text=True)

    def test_x64_named_folder_can_hold_valid_x86_archive(self):
        with tempfile.TemporaryDirectory() as directory:
            result = self.configure(Path(directory), 'x86', archive(('one.obj/', coff(0x14c))), 'x64')
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_x86_named_folder_cannot_hide_arm64_archive(self):
        with tempfile.TemporaryDirectory() as directory:
            result = self.configure(Path(directory), 'x86', archive(('one.obj/', coff(0xaa64))), 'x86')
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('arm64', result.stdout + result.stderr)

    def test_mixed_archive_cannot_satisfy_x64_target(self):
        with tempfile.TemporaryDirectory() as directory:
            payload = archive(('one.obj/', coff(0x8664)), ('two.obj/', coff(0x14c)))
            result = self.configure(Path(directory), 'x64', payload, 'provider')
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('Mixed', result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
