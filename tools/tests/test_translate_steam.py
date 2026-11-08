"""Native Steam routing retains one source inventory and the Win32 reference import."""
from pathlib import Path
import importlib.util
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
SPEC = importlib.util.spec_from_file_location('translate_msbuild', ROOT / 'tools/translate_msbuild.py')
translator = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(translator)
OWNERS = {
    'hw_engine': 'projects/vs2026/hw_engine.vcxproj',
    'swds': 'swds/swds.vcxproj',
    'gameui': 'projects/vs2026/GameUI.vcxproj',
    'cs_client': 'projects/vs2026/cstrike_cdll.vcxproj',
    'hltv_proxy': 'projects/vs2026/Proxy.vcxproj',
}


class SteamTranslationTests(unittest.TestCase):
    def test_pinned_reference_import_uses_architecture_policy(self):
        path = ROOT / 'GoldSrc-SDK/lib/public/steam_api.lib'
        self.assertEqual(translator.import_command('hw_engine', 'Release', path, {}),
                         'cse_link_steam(hw_engine Release "${CSE_SDK_DIR}/lib/public/steam_api.lib")')

    def test_unowned_import_path_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Unpinned Steam'):
            translator.import_command('hw_engine', 'Release', ROOT / 'foreign/steam_api.lib', {})

    def test_every_consumer_owns_the_same_guarded_service_source(self):
        expected = ROOT / 'GoldSrc-SDK/common/steam_service.cpp'
        for target, project in OWNERS.items():
            for configuration in ['Release', 'Debug']:
                with self.subTest(target=target, configuration=configuration):
                    data = translator.evaluate(ROOT / 'GoldSrc-SDK' / project, configuration)
                    active = [item['path'] for item in data['items'] if item['kind'] == 'ClCompile'
                              and item['metadata'].get('ExcludedFromBuild') != 'true']
                    self.assertEqual(active.count(str(expected)), 1)

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
