import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tests'))
from test_binary_identity import archive, coff, macos_object
from audit_native_dependencies import audit_dependencies


class NativeDependencyTests(unittest.TestCase):
    def fixture(self, root):
        reference = root / 'reference'
        installed = root / 'installed'
        document = {
            'packages': [
                {'SPDXID': 'SPDXRef-port', 'name': 'freetype', 'versionInfo': '2.14.3'},
                {'SPDXID': 'SPDXRef-resource-0', 'downloadLocation': 'https://example.test/source',
                 'checksums': [{'algorithm': 'SHA512', 'checksumValue': 'a' * 128}]}
            ],
            'files': [{'fileName': './portfile.cmake', 'SPDXID': 'SPDXRef-port-file-0',
                       'checksums': [{'algorithm': 'SHA256', 'checksumValue': 'b' * 64}]}]
        }
        for directory in [reference, installed / 'share']:
            (directory / 'freetype').mkdir(parents=True)
            (directory / 'freetype/vcpkg.spdx.json').write_text(json.dumps(document))
        for configuration in ['lib', 'debug/lib']:
            path = installed / configuration
            path.mkdir(parents=True)
            (path / 'freetype.lib').write_bytes(archive(('ft.obj/', coff(0x8664))))
        return reference, installed, document

    def test_matching_recipes_and_both_machine_checked_configurations(self):
        with tempfile.TemporaryDirectory() as directory:
            reference, installed, _ = self.fixture(Path(directory))
            report = audit_dependencies(installed, reference, ['freetype'], 'x64')
            self.assertEqual(len(report['binaries']), 2)
            self.assertEqual(report['packages']['freetype']['version'], '2.14.3')
            self.assertFalse(report['qualified'])

    def test_different_recipe_or_source_hash_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            reference, installed, original = self.fixture(Path(directory))
            for field in ['files', 'packages']:
                changed = copy.deepcopy(original)
                if field == 'files':
                    changed[field][0]['checksums'][0]['checksumValue'] = 'c' * 64
                else:
                    changed[field][1]['checksums'][0]['checksumValue'] = 'd' * 128
                (installed / 'share/freetype/vcpkg.spdx.json').write_text(json.dumps(changed))
                with self.subTest(field=field), self.assertRaisesRegex(ValueError, 'source|recipe'):
                    audit_dependencies(installed, reference, ['freetype'], 'x64')

    def test_platform_binary_metadata_does_not_change_source_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            reference, installed, document = self.fixture(Path(directory))
            document['files'].append({'fileName': './lib/freetype.lib', 'SPDXID': 'SPDXRef-binary-file-0',
                                      'checksums': [{'algorithm': 'SHA256', 'checksumValue': 'e' * 64}]})
            (installed / 'share/freetype/vcpkg.spdx.json').write_text(json.dumps(document))
            report = audit_dependencies(installed, reference, ['freetype'], 'x64')
            self.assertEqual(report['packages']['freetype']['recipe_files'], 1)

    def test_foreign_debug_archive_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            reference, installed, _ = self.fixture(Path(directory))
            (installed / 'debug/lib/freetype.lib').write_bytes(archive(('ft.obj/', coff(0xaa64))))
            with self.assertRaisesRegex(ValueError, 'arm64'):
                audit_dependencies(installed, reference, ['freetype'], 'x64')

    def test_missing_debug_or_required_package_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            reference, installed, _ = self.fixture(Path(directory))
            (installed / 'debug/lib/freetype.lib').unlink()
            with self.assertRaisesRegex(ValueError, 'Debug'):
                audit_dependencies(installed, reference, ['freetype'], 'x64')
            with self.assertRaises((ValueError, FileNotFoundError)):
                audit_dependencies(installed, reference, ['zlib'], 'x64')

    def test_macos_archives_cannot_inherit_the_builder_os_minimum(self):
        with tempfile.TemporaryDirectory() as directory:
            reference, installed, _ = self.fixture(Path(directory))
            for configuration in ['lib', 'debug/lib']:
                (installed / configuration / 'freetype.lib').write_bytes(archive(('ft.o/', macos_object(27))))
            with self.assertRaisesRegex(ValueError, 'macOS'):
                audit_dependencies(installed, reference, ['freetype'], 'arm64', maximum_macos=(14, 0, 0))


if __name__ == '__main__':
    unittest.main()
