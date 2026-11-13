"""Both compiler branches evaluate one owning CMake game-source inventory."""
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from cmake_inventory import query_target


class PortableGameGenerationTests(unittest.TestCase):
    def test_game_source_membership_agrees_across_configurations_and_compilers(self):
        for target in ('hl_server', 'cs_server'):
            selections = []
            for configuration in ('Debug', 'Release'):
                for target_os, architecture in (('windows', 'x86'), ('macos', 'arm64')):
                    data = query_target(ROOT, target, configuration, target_os, architecture)
                    selected = {item['path'] for item in data['sources'] if item['language'] in ('C', 'CXX')}
                    self.assertTrue(selected)
                    self.assertTrue(all(Path(path).is_file() for path in selected))
                    if target_os != 'windows' and target == 'cs_server':
                        wide_format = str(ROOT / 'src/common/legacy_wide_format.cpp')
                        self.assertIn(wide_format, selected)
                        selected.remove(wide_format)
                    selections.append(selected)
            self.assertTrue(all(selection == selections[0] for selection in selections), target)

    def test_per_file_engine_overrides_keep_exception_and_float_policy(self):
        for configuration in ('Debug', 'Release'):
            for target_os, architecture, exception, floating in (('windows', 'x86', '/EHsc', '/fp:precise'),
                                                                 ('macos', 'arm64', '-fexceptions', '-ffp-model=precise')):
                with self.subTest(configuration=configuration, target_os=target_os):
                    data = query_target(ROOT, 'hw_engine', configuration, target_os, architecture)
                    for filename in ('mpg123_decoder.cpp', 'network_worker.cpp'):
                        source = next(item for item in data['sources'] if Path(item['path']).name == filename)
                        self.assertIn(exception, source['compile_options'])
                        self.assertIn(floating, source['compile_options'])
                        if target_os == 'macos':
                            exceptions = [flag for flag in source['compile_options']
                                          if flag in ('-fexceptions', '-fno-exceptions')]
                            self.assertEqual(exceptions[-1], '-fexceptions')


if __name__ == '__main__':
    unittest.main()
