"""Inventory queries evaluate the production CMake compiler recipes on the host."""
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from cmake_inventory import query_target, query_targets


class CMakeInventoryTests(unittest.TestCase):
    def test_default_cs_server_reports_stable_regamedll_source_paths(self):
        upstream = ROOT / 'src/external/regamedll'
        integration = ROOT / 'src/cstrike/regamedll'
        for target_os, architecture in (('windows', 'x86'), ('windows', 'x64'),
                                        ('windows', 'arm64'), ('macos', 'arm64')):
            with self.subTest(target_os=target_os, architecture=architecture):
                data = query_target(ROOT, 'cs_server', target_os=target_os, architecture=architecture)
                self.assertEqual(data['properties'].get('CSE_CS_SERVER_BACKEND'), 'regamedll')
                self.assertEqual(data['source_overlay']['root'], str(upstream))
                self.assertEqual(data['source_overlay']['patch'], str(integration / 'cse.patch'))
                self.assertEqual(data['source_overlay']['revision'], '1f16a7f25c14225931f8df1abc51f68d18ded839')
                compiled = {Path(item['path']): item for item in data['sources'] if item['language'] == 'CXX'}
                cbase = upstream / 'regamedll/dlls/cbase.cpp'
                self.assertIn(cbase, compiled)
                self.assertIn(str(cbase.parent), compiled[cbase]['include_directories'])
                self.assertEqual(compiled[cbase].get('generated'),
                                 {'source': str(cbase), 'registration': 'dlls_cbase'})
                airbox = upstream / 'regamedll/dlls/addons/item_airbox.cpp'
                self.assertEqual(compiled[airbox].get('generated'),
                                 {'source': str(airbox), 'registration': 'dlls_addons_item_airbox'})
                self.assertIn(integration / 'game_module_entry.cpp', compiled)
                self.assertEqual(len(compiled), sum(item['language'] == 'CXX' for item in data['sources']))
                self.assertTrue(all(path.is_file() for path in compiled))
                definitions = [item for item in data['sources'] if Path(item['path']).suffix == '.def']
                if target_os == 'windows':
                    self.assertEqual(len(definitions), 1)
                    definition = definitions[0]['generated']
                    self.assertEqual(definition['overlay_path'], 'regamedll/cse-exports.def')
                    self.assertEqual(definition['source'], str(ROOT / 'cmake/exports/cs_server.def'))
                    dialog = upstream / 'regamedll/public/tier0/assert_dialog.rc'
                    resource = next(item for item in data['sources'] if item['language'] == 'RC')
                    self.assertEqual(resource['path'], str(dialog))
                    self.assertIn(str(dialog.parent), resource['include_directories'])
                else:
                    self.assertFalse(definitions)

    def test_legacy_cs_server_is_selectable_and_remains_the_linux_inventory(self):
        for options in ({'cs_server_backend': 'legacy'}, {'target_os': 'linux', 'architecture': 'x64'}):
            with self.subTest(options=options):
                data = query_target(ROOT, 'cs_server', **options)
                self.assertEqual(data['properties'].get('CSE_CS_SERVER_BACKEND'), 'legacy')
                self.assertNotIn('source_overlay', data)
                self.assertIn(str(ROOT / 'src/cstrike/dlls/cbase.cpp'),
                              {item['path'] for item in data['sources']})

    def test_hw_engine_exposes_selected_sources_and_effective_float_policy(self):
        target = query_target(ROOT, 'hw_engine')
        self.assertEqual(target['target'], 'hw_engine')
        self.assertEqual(target['runtime']['kind'], 'SHARED')
        source = next(item for item in target['sources']
                      if item['path'].endswith('/engine/gl_vidnt.cpp'))
        self.assertEqual(source['language'], 'CXX')
        self.assertIn('/fp:precise', source['compile_options'])
        self.assertTrue(Path(source['path']).is_absolute())


    def test_a3d_generated_descriptors_retain_the_original_source_and_section(self):
        expected = {'A3D_GEOMETRY_OBJECTS', 'A3D_GEOMETRY_FINALIZE', 'A3D_GEOMETRY_ACOUSTICS',
                    'A3D_GEOMETRY_SUBMISSION', 'A3D_GEOMETRY_STATE'}
        for configuration in ('Debug', 'Release'):
            data = query_target(ROOT, 'a3dapi', configuration)
            generated = [item for item in data['sources'] if item.get('generated')]
            self.assertEqual(len(generated), 5)
            self.assertEqual({item['generated']['section'] for item in generated}, expected)
            for source in generated:
                self.assertEqual(source['language'], 'CXX')
                self.assertEqual(source['generated']['source'], str(ROOT / 'src/a3dapi/A3dGeom.cpp'))
                self.assertTrue(Path(source['generated']['source']).is_file())

    def test_windows_recipes_exclude_host_compiler_default_flags(self):
        for configuration in ('Debug', 'Release'):
            for data in query_targets(ROOT, configuration):
                for source in data['sources']:
                    self.assertFalse(any(flag.startswith('-') for flag in source['compile_options']),
                                     data['target'] + '/' + source['path'])

    def test_resource_recipe_includes_its_source_directory_and_windows_definitions(self):
        for configuration in ('Debug', 'Release'):
            data = query_target(ROOT, 'a3dapi', configuration)
            resource = next(item for item in data['sources'] if item['language'] == 'RC')
            self.assertEqual(resource['path'], str(ROOT / 'src/a3dapi/a3dapi.rc'))
            self.assertIn(str(ROOT / 'src/a3dapi'), resource['include_directories'])
            self.assertIn('WIN32', resource['compile_definitions'])
            self.assertIn('_WINDOWS', resource['compile_definitions'])
            self.assertIn('_DEBUG' if configuration == 'Debug' else 'NDEBUG', resource['compile_definitions'])

    def test_runtime_output_directories_are_relative_to_the_runtime_root(self):
        for target_os, architecture in (('windows', 'x86'), ('macos', 'arm64')):
            for data in query_targets(ROOT, target_os=target_os, architecture=architecture):
                directory = data['runtime']['directory']
                self.assertFalse(Path(directory).is_absolute(), data['target'])
                self.assertNotIn('..', Path(directory).parts, data['target'])
                self.assertNotIn('\\', directory, data['target'])


if __name__ == '__main__':
    unittest.main()
