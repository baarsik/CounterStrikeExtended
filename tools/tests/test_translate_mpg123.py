"""Source-built mpg123 must translate as a producer, never a prebuilt import."""
from pathlib import Path
import importlib.util
import tempfile
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
SPEC = importlib.util.spec_from_file_location('translate_msbuild', ROOT / 'tools/translate_msbuild.py')
translator = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(translator)


class Mpg123TranslationTests(unittest.TestCase):
    def test_generated_library_uses_source_producer(self):
        library = ROOT / 'GoldSrc-SDK/bin/obj/engdll/Win32/Release/mpg123/lib/Release/mpg123/mpg123.lib'
        data = {'mpg123': {'import_library': str(library)}}
        self.assertEqual(translator.import_command('hw_engine', 'Release', library, data),
                         'cse_link_mpg123(hw_engine Release)')

    def test_retained_engines_resolve_source_build_for_both_configurations(self):
        sdk = ROOT / 'GoldSrc-SDK'
        for project, target in [('projects/vs2026/hw_engine.vcxproj', 'hw_engine'), ('swds/swds.vcxproj', 'swds')]:
            for configuration in ['Debug', 'Release']:
                with self.subTest(project=project, configuration=configuration):
                    data = translator.evaluate(sdk / project, configuration)
                    library = Path(data['mpg123']['import_library'])
                    self.assertEqual(translator.import_command(target, configuration, library, data),
                                     f'cse_link_mpg123({target} {configuration})')

    def test_modified_build_actions_fail_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'Mpg123.targets'
            original = (ROOT / 'GoldSrc-SDK/projects/vs2026/Mpg123.targets').read_text()
            path.write_text(original.replace('<MakeDir ', '<Exec Command="unrecognized" /><MakeDir '))
            with self.assertRaisesRegex(ValueError, 'Unsupported mpg123 build contract'):
                translator.validate_mpg123_build_file(path, 'targets')

    def test_generated_library_requires_its_source_build_contract(self):
        with self.assertRaisesRegex(ValueError, 'Unpinned mpg123'):
            translator.import_command('hw_engine', 'Release', ROOT / 'unselected/mpg123.lib', {})


if __name__ == '__main__':
    unittest.main()
