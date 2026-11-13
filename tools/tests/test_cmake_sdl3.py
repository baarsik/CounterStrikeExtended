"""Production target recipes select SDL3 only for consumers that use it."""
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from cmake_inventory import query_target


class Sdl3CMakeTests(unittest.TestCase):
    def test_shared_consumers_link_the_provider_for_both_configurations(self):
        for target in ('hw_engine', 'swds', 'cs_client', 'hl_client', 'gameui'):
            for configuration in ('Debug', 'Release'):
                for target_os, architecture in (('windows', 'x86'), ('macos', 'arm64')):
                    with self.subTest(target=target, configuration=configuration, target_os=target_os):
                        data = query_target(ROOT, target, configuration, target_os, architecture)
                        self.assertTrue(any('sdl3' in item.lower() for item in data['link_libraries']),
                                        data['link_libraries'])
                        self.assertTrue(any('SDL3/include' in path
                                            for item in data['sources']
                                            for path in item['include_directories']))

    def test_header_only_particleman_does_not_acquire_a_windows_provider(self):
        for configuration in ('Debug', 'Release'):
            data = query_target(ROOT, 'particleman', configuration)
            self.assertFalse(any('sdl3' in item.lower() for item in data['link_libraries']))
            self.assertTrue(any('SDL3/include' in path for item in data['sources']
                                for path in item['include_directories']))


if __name__ == '__main__':
    unittest.main()
