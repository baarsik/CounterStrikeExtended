"""The dedicated launcher selects SDL3 on native POSIX targets only."""
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from cmake_inventory import query_target


class NativeSdlPolicyTests(unittest.TestCase):
    def test_hlds_provider_follows_target_platform_for_both_configurations(self):
        for configuration in ('Debug', 'Release'):
            for target_os, architecture, expected in (('windows', 'x86', False),
                                                       ('windows', 'x64', False),
                                                       ('macos', 'arm64', True),
                                                       ('linux', 'x64', True)):
                with self.subTest(configuration=configuration, target_os=target_os, architecture=architecture):
                    data = query_target(ROOT, 'hlds', configuration, target_os, architecture)
                    self.assertEqual(any('sdl3' in item.lower() for item in data['link_libraries']), expected)

    def test_unrequested_game_server_does_not_acquire_sdl3(self):
        for target_os, architecture in (('windows', 'x86'), ('macos', 'arm64')):
            data = query_target(ROOT, 'cs_server', 'Release', target_os, architecture)
            self.assertFalse(any('sdl3' in item.lower() for item in data['link_libraries']))


if __name__ == '__main__':
    unittest.main()
