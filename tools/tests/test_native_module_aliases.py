"""Native module names honor retained game metadata without changing Windows outputs."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import translate_msbuild as translator


class NativeModuleAliasesTests(unittest.TestCase):
    def fixture(self):
        return {config: {'properties': {'ConfigurationType': 'DynamicLibrary',
                                       'CseNativeModuleAliases': 'cs'}} for config in ('Debug', 'Release')}

    def test_alias_is_native_policy_with_one_retained_output(self):
        lines = []
        translator.emit_native_module_aliases(lines, 'cs_server', self.fixture())
        self.assertEqual(lines, ['if(NOT WIN32)', '  cse_native_module_alias(cs_server "cs")', 'endif()'])

    def test_alias_configurations_must_agree(self):
        configs = self.fixture()
        configs['Debug']['properties']['CseNativeModuleAliases'] = 'other'
        with self.assertRaisesRegex(ValueError, 'configuration'):
            translator.emit_native_module_aliases([], 'cs_server', configs)

    def test_alias_rejects_paths_duplicate_names_and_executable_targets(self):
        for alias in ('../cs', 'dlls/cs', 'cs\\other', 'cs;CS'):
            configs = self.fixture()
            for config in configs.values():
                config['properties']['CseNativeModuleAliases'] = alias
            with self.subTest(alias=alias), self.assertRaises(ValueError):
                translator.emit_native_module_aliases([], 'cs_server', configs)
        configs = self.fixture()
        for config in configs.values():
            config['properties']['ConfigurationType'] = 'Application'
        with self.assertRaisesRegex(ValueError, 'library'):
            translator.emit_native_module_aliases([], 'launcher', configs)

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
