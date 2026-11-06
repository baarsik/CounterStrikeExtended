"""Narrow SDL3 translation contract; unrelated custom actions remain errors."""
from pathlib import Path
import importlib.util
import tempfile
import shutil
import json
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
SPEC = importlib.util.spec_from_file_location('translate_msbuild', ROOT / 'tools/translate_msbuild.py')
translator = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(translator)
SDK = ROOT / 'GoldSrc-SDK'


class TranslationTests(unittest.TestCase):
    def project(self, shared='true', extra=''):
        return f'''<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup><CseSdl3Shared>{shared}</CseSdl3Shared></PropertyGroup>
  <Import Project="{SDK}/projects/vs2026/SDL3.props" />
  <Import Project="{SDK}/projects/vs2026/SDL3.targets" />{extra}
</Project>'''

    def evaluate(self, content):
        with tempfile.TemporaryDirectory(dir=SDK) as directory:
            path = Path(directory) / 'fixture.vcxproj'
            path.write_text(content)
            return translator.evaluate(path, 'Release')

    def test_import_file_directory_and_shared_metadata(self):
        data = self.evaluate(self.project())
        self.assertEqual(data['sdl3']['runtime_root'], str(SDK / 'bin/Win32/Release'))
        self.assertEqual(data['sdl3']['runtime_dll'], str(SDK / 'external/SDL3/lib/x86/SDL3.dll'))
        self.assertTrue(data['sdl3']['shared'])
        self.assertIn('SDL3.lib', data['definitions']['Link']['AdditionalDependencies'])
        self.assertNotIn('MSBuildThisFileDirectory', data['properties'])

    def test_header_only_does_not_acquire_import_or_staging(self):
        data = self.evaluate(self.project('false'))
        self.assertFalse(data['sdl3']['shared'])
        self.assertNotIn('Link', data['definitions'])

    def test_only_exact_import_is_native_shared_library(self):
        path = SDK / 'external/SDL3/lib/x86/SDL3.lib'
        data = self.evaluate(self.project())
        self.assertEqual(translator.import_command('test', 'Release', path, data),
                         'cse_link_sdl3(test Release)')
        for other in [SDK / 'lib/SDL3.lib', SDK / 'external/SDL3/lib/x64/SDL3.lib']:
            with self.subTest(path=other), self.assertRaisesRegex(ValueError, 'Unpinned SDL3'):
                translator.import_command('test', 'Release', other, data)
        with self.assertRaisesRegex(ValueError, 'shared build contract'):
            translator.import_command('test', 'Release', path, {})

    def test_unknown_target_and_custom_task_fail(self):
        for action in ['<Target Name="Unexpected"><Exec Command="anything" /></Target>',
                       '<UsingTask TaskName="Unexpected" AssemblyFile="anything" />']:
            with self.subTest(action=action), self.assertRaisesRegex(ValueError, 'Unsupported'):
                self.evaluate(self.project(extra=action))

    def test_generation_records_shared_dependency_and_staging(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            sdk = root / 'GoldSrc-SDK'
            shared = sdk / 'projects/vs2026'
            shared.mkdir(parents=True)
            shutil.copytree(SDK / 'external/SDL3', sdk / 'external/SDL3')
            for kind in ['props', 'targets']:
                shutil.copyfile(SDK / f'projects/vs2026/SDL3.{kind}', shared / f'SDL3.{kind}')
            (sdk / 'stub.c').write_text('int probe(void) { return 0; }\n')
            (sdk / 'fixture.vcxproj').write_text(f'''<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup><CseSdl3Shared>true</CseSdl3Shared><ConfigurationType>DynamicLibrary</ConfigurationType></PropertyGroup>
  <Import Project="{shared}/SDL3.props" />
  <ItemDefinitionGroup><ClCompile><RuntimeLibrary>MultiThreadedDLL</RuntimeLibrary></ClCompile></ItemDefinitionGroup>
  <ItemGroup><ClCompile Include="stub.c" /></ItemGroup>
  <Import Project="{shared}/SDL3.targets" />
</Project>''')
            (root / 'docs/msbuild').mkdir(parents=True)
            with patch.multiple(translator, ROOT=root, SDK=sdk, TARGETS={'fixture': 'fixture'}, CASE_PATHS={}):
                translator.main()
            generated = (root / 'cmake/targets/fixture/CMakeLists.txt').read_text()
            for configuration in ['Debug', 'Release']:
                self.assertIn(f'cse_link_sdl3(fixture {configuration})', generated)
            self.assertNotIn('cse_link_import', generated)
            selections = json.loads((root / 'docs/msbuild/repository-selections.json').read_text())
            data = selections[0]['configurations']['Release']['sdl3']
            self.assertEqual(data['runtime_root'], '${CSE_SDK_DIR}/bin/Win32/Release')
            self.assertEqual(data['staging_target'], 'CseStageSdl3')
            self.assertEqual(data['validation_target'], 'CseValidateSdl3')

    def test_modified_sdl3_actions_fail_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'SDL3.targets'
            original = (SDK / 'projects/vs2026/SDL3.targets').read_text()
            path.write_text(original.replace('<MakeDir ', '<Exec Command="unrecognized" /><MakeDir '))
            with self.assertRaisesRegex(ValueError, 'Unsupported SDL3 build contract'):
                translator.validate_sdl3_build_file(path, 'targets')


if __name__ == '__main__':
    unittest.main()
