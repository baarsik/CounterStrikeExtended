"""Steam consumers keep one service source and the provider's fail-closed checks."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from cmake_inventory import query_target

OWNERS = ('hw_engine', 'swds', 'gameui', 'cs_client', 'hltv_proxy')


class SteamCMakeTests(unittest.TestCase):
    def test_consumers_share_one_service_source_and_link_a_provider(self):
        expected = str(ROOT / 'src/common/steam_service.cpp')
        for target in OWNERS:
            for configuration in ('Release', 'Debug'):
                for target_os, architecture in (('windows', 'x86'), ('macos', 'arm64')):
                    with self.subTest(target=target, configuration=configuration, target_os=target_os):
                        data = query_target(ROOT, target, configuration, target_os, architecture)
                        self.assertEqual([item['path'] for item in data['sources']].count(expected), 1)
                        if architecture == 'x86':
                            self.assertIn(str(ROOT / 'src/lib/public/steam_api.lib'), data['link_libraries'])
                        else:
                            service = next(item for item in data['sources'] if item['path'] == expected)
                            self.assertIn('CSE_OWNED_STEAM_PROVIDER=1', service['compile_definitions'])

    def test_unverified_native_payload_and_conditional_provider_fail_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'steam_api64.dll'
            path.write_bytes(b'unverified native payload')
            for target_os, architecture, diagnostic in [('windows', 'x64', 'Pinned Steam provider'),
                                                         ('windows', 'arm64', 'No qualified Steam provider')]:
                with self.subTest(target_os=target_os, architecture=architecture):
                    result = subprocess.run(['cmake', f'-DCSE_STEAM_PROVIDER_FILE={path}',
                        f'-DCSE_TARGET_OS={target_os}', f'-DCSE_TARGET_ARCH={architecture}',
                        '-P', str(ROOT / 'cmake/SteamDependency.cmake')], capture_output=True, text=True)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertIn(diagnostic, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
