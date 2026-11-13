"""Both engine recipes route mpg123 through the source-built CMake provider."""
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from cmake_inventory import query_target


class Mpg123CMakeTests(unittest.TestCase):
    def test_engine_recipes_use_source_producer_for_each_platform_and_configuration(self):
        for target in ('hw_engine', 'swds'):
            for configuration in ('Debug', 'Release'):
                for target_os, architecture in (('windows', 'x86'), ('macos', 'arm64')):
                    with self.subTest(target=target, configuration=configuration, target_os=target_os):
                        data = query_target(ROOT, target, configuration, target_os, architecture)
                        libraries = data['link_libraries']
                        self.assertTrue(any('mpg123' in item for item in libraries), libraries)
                        self.assertFalse(any(item.lower().endswith('mpg123.lib') for item in libraries), libraries)
                        sources = {Path(item['path']).name for item in data['sources']}
                        self.assertIn('mpg123_decoder.cpp', sources)
                        self.assertIn('mpg123_music.cpp', sources)


if __name__ == '__main__':
    unittest.main()
