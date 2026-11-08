"""Platform-only source ownership stays in retained manifests, outside compiler policy."""
from copy import deepcopy
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import translate_msbuild as translator


class PlatformSourceMembershipTests(unittest.TestCase):
    def fixture(self):
        item = {'kind': 'ClCompile', 'path': str(ROOT / 'GoldSrc-SDK/example.cpp'),
                'metadata': {'ExcludedFromBuild': 'true', 'CseTargetOS': 'macos'},
                'overrides': {'ExcludedFromBuild': 'true', 'CseTargetOS': 'macos'}}
        return {config: {'items': [deepcopy(item)]} for config in ('Debug', 'Release')}

    def test_explicit_platform_source_remains_conditional(self):
        lines = []
        translator.emit_platform_sources(lines, 'fixture', self.fixture())
        text = '\n'.join(lines)
        self.assertIn('if(APPLE)', text)
        self.assertIn('target_sources(fixture PRIVATE "${CSE_SDK_DIR}/example.cpp")', text)
        self.assertNotIn('/MACHINE', text)
        self.assertNotIn('target_compile_options', text)

    def test_unsupported_platform_is_rejected(self):
        configs = self.fixture()
        for config in configs.values():
            config['items'][0]['metadata']['CseTargetOS'] = 'host'
        with self.assertRaisesRegex(ValueError, 'CseTargetOS'):
            translator.emit_platform_sources([], 'fixture', configs)

    def test_platform_membership_must_match_both_configurations(self):
        configs = self.fixture()
        configs['Debug']['items'][0]['metadata']['CseTargetOS'] = 'linux'
        with self.assertRaisesRegex(ValueError, 'configuration'):
            translator.emit_platform_sources([], 'fixture', configs)

    def test_native_only_source_cannot_leak_into_reference_selection(self):
        configs = self.fixture()
        configs['Release']['items'][0]['metadata']['ExcludedFromBuild'] = 'false'
        with self.assertRaisesRegex(ValueError, 'Win32'):
            translator.emit_platform_sources([], 'fixture', configs)


if __name__ == '__main__':
    unittest.main()
