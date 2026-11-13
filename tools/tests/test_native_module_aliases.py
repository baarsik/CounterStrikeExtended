"""Native module aliases use relative names and preserve unowned files."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))


class NativeModuleAliasesTests(unittest.TestCase):
    def test_real_alias_helper_rejects_paths_and_non_library_owners(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'stub.cpp').write_text('int main() { return 0; }\n')
            cases = [('parent', '../cs', 'SHARED'), ('directory', 'dlls/cs', 'SHARED'),
                     ('backslash', 'cs\\other', 'SHARED'), ('multiple', 'cs;CS', 'SHARED'),
                     ('executable', 'cs', 'EXECUTABLE')]
            for label, alias, kind in cases:
                with self.subTest(case=label):
                    declaration = ('add_executable(fixture stub.cpp)' if kind == 'EXECUTABLE'
                                   else 'add_library(fixture SHARED stub.cpp)')
                    (root / 'CMakeLists.txt').write_text(
                        'cmake_minimum_required(VERSION 3.25)\nproject(AliasPolicy CXX)\nset(WIN32 FALSE)\n'
                        f'include("{ROOT}/cmake/RuntimeTargets.cmake")\n{declaration}\n'
                        f'cse_native_module_alias(fixture [[{alias}]])\n')
                    result = subprocess.run(['cmake', '-S', str(root), '-B', str(root / label), '-G', 'Ninja'],
                                            capture_output=True, text=True)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn('Invalid native module alias', result.stdout + result.stderr)

    def run_script(self, alias, target):
        return subprocess.run(['cmake', f'-DCSE_MODULE_ALIAS={alias}', f'-DCSE_MODULE_TARGET={target}',
                               '-P', str(ROOT / 'cmake/RuntimeModuleAlias.cmake')], capture_output=True, text=True)

    @unittest.skipIf(sys.platform == 'win32', 'Runtime aliases are POSIX-only')
    def test_alias_is_relative_idempotent_and_does_not_replace_unowned_files(self):
        with tempfile.TemporaryDirectory(prefix='cse-module-alias-') as directory:
            root = Path(directory)
            target = root / 'mp.dylib'
            target.write_bytes(b'owned first build')
            alias = root / 'cs.dylib'
            result = self.run_script(alias, target.name)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertTrue(alias.is_symlink())
            self.assertEqual(alias.readlink(), Path(target.name))
            target.write_bytes(b'owned rebuilt image')
            result = self.run_script(alias, target.name)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(alias.read_bytes(), target.read_bytes())
            alias.unlink()
            alias.write_bytes(b'unowned existing module')
            result = self.run_script(alias, target.name)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(alias.read_bytes(), b'unowned existing module')
            alias.unlink()
            alias.symlink_to('different.dylib')
            result = self.run_script(alias, target.name)
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(alias.readlink(), Path('different.dylib'))


if __name__ == '__main__':
    unittest.main()
