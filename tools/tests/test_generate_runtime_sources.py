"""Derived runtime files follow the owning CMake source and export selections."""
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from cmake_inventory import query_target
from generate_runtime_sources import callback_sources, export_paths


class GenerateRuntimeSourcesTests(unittest.TestCase):
    def copy_cmake(self, directory):
        root = Path(directory).resolve()
        shutil.copytree(ROOT / 'cmake', root / 'cmake')
        (root / 'src').symlink_to(ROOT / 'src', target_is_directory=True)
        return root

    def test_callback_membership_matches_both_legacy_configurations(self):
        for target in ('hl_server', 'cs_server'):
            with self.subTest(target=target):
                actual = {Path(path) for path in callback_sources(ROOT, target)}
                self.assertTrue(actual)
                self.assertTrue(any(path.name == 'airtank.cpp' for path in actual))
                for configuration in ('Debug', 'Release'):
                    data = query_target(ROOT, target, configuration, cs_server_backend='legacy')
                    selected = {Path(item['path']) for item in data['sources'] if item['language'] in ('C', 'CXX')}
                    self.assertEqual(actual, selected)

    def test_export_destinations_match_the_legacy_native_selection(self):
        for target in ('hl_server', 'cs_server', 'hl_client', 'cs_client', 'particleman', 'swds', 'a3dapi'):
            with self.subTest(target=target):
                source, destination = map(Path, export_paths(ROOT, target))
                self.assertTrue(source.is_file())
                self.assertEqual(destination, ROOT / 'cmake/exports' / (target + '.def'))
                native = query_target(ROOT, target, 'Release', 'windows', 'x64', cs_server_backend='legacy')
                selected = [Path(item['path']) for item in native['sources'] if Path(item['path']).suffix == '.def']
                self.assertEqual(selected, [destination])

    def test_configuration_divergence_cannot_generate_one_shared_callback_catalog(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.copy_cmake(directory)
            manifest = root / 'cmake/targets/cs_server/Sources.cmake'
            original = '"${CSE_SDK_DIR}/cstrike/dlls/airtank.cpp"'
            conditional = '"$<$<CONFIG:Debug>:${CSE_SDK_DIR}/cstrike/dlls/airtank.cpp>"'
            text = manifest.read_text()
            self.assertEqual(text.count(original), 1)
            manifest.write_text(text.replace(original, conditional))
            with self.assertRaisesRegex(ValueError, '(?i)(configuration|debug|release)'):
                callback_sources(root, 'cs_server')

    def test_native_export_destination_follows_a_moved_cmake_definition(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.copy_cmake(directory)
            original = root / 'cmake/exports/cs_server.def'
            moved = root / 'cmake/exports/server_native_exports.def'
            original.rename(moved)
            recipe = root / 'cmake/targets/cs_server/CMakeLists.txt'
            text = recipe.read_text()
            old_path = '${CSE_ROOT}/cmake/exports/cs_server.def'
            new_path = '${CSE_ROOT}/cmake/exports/server_native_exports.def'
            self.assertEqual(text.count(old_path), 1)
            recipe.write_text(text.replace(old_path, new_path))
            source, destination = map(Path, export_paths(root, 'cs_server'))
            self.assertEqual(source, root / 'cmake/exports/win32/cstrikedll.def')
            self.assertEqual(destination, moved)
            self.assertFalse(original.exists())


    def test_missing_derived_definition_still_has_a_queryable_regeneration_destination(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.copy_cmake(directory)
            destination = root / 'cmake/exports/cs_server.def'
            destination.unlink()
            source, selected_destination = map(Path, export_paths(root, 'cs_server'))
            self.assertEqual(source, root / 'cmake/exports/win32/cstrikedll.def')
            self.assertEqual(selected_destination, destination)
            self.assertTrue(source.is_file())
            self.assertFalse(destination.exists())

    def test_missing_original_definition_cannot_be_treated_as_a_generated_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.copy_cmake(directory)
            (root / 'cmake/exports/win32/cstrikedll.def').unlink()
            with self.assertRaises(ValueError):
                export_paths(root, 'cs_server')


if __name__ == '__main__':
    unittest.main()
