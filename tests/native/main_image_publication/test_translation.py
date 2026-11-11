"""Behavioral checks at the retained-source to executable-link boundary."""
import copy
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools'))
import translate_msbuild as translator

PROJECTS = {'launcher': 'launcher/launcher.vcxproj', 'hlds': 'projects/vs2026/hlds.vcxproj'}
KEY = 'CseAppleMainImagePublisher'


def configurations(owner):
    return {name: translator.evaluate(translator.SDK / PROJECTS[owner], name) for name in ('Debug', 'Release')}


def owner_item(config):
    return next(item for item in config['items']
                if Path(item['path']) == translator.SDK / 'launcher/native/SocketImageOwner.cpp')


class TranslationTests(unittest.TestCase):
    def test_retained_owners_require_same_version(self):
        for owner in PROJECTS:
            data = configurations(owner)
            for config in data.values():
                self.assertEqual(owner_item(config)['metadata'].get(KEY), 'socket-v1')

    def test_invalid_metadata_never_silently_disappears(self):
        mutations = [('version', {KEY: 'socket-v2'}), ('platform', {'CseTargetOS': 'linux'}),
                     ('width', {'CsePointerWidth': ''}), ('included', {'ExcludedFromBuild': 'false'}),
                     ('service', {'CseNativeServices': 'font-registration-owner'})]
        for label, change in mutations:
            with self.subTest(case=label):
                data = configurations('launcher')
                for config in data.values():
                    owner_item(config)['metadata'].update({KEY: 'socket-v1', **change})
                with self.assertRaises(ValueError):
                    translator.emit_platform_sources([], 'launcher', data)
        for label in ('configuration', 'source', 'kind', 'owner', 'library', 'duplicate', 'missing', 'metadata'):
            with self.subTest(case=label):
                data = configurations('launcher')
                for config in data.values():
                    item = owner_item(config)
                    item['metadata'][KEY] = 'socket-v1'
                    if label == 'source':
                        item['path'] = str(translator.SDK / 'launcher/native/MacRuntimeSession.cpp')
                    if label == 'kind':
                        item['kind'] = 'None'
                    if label == 'library':
                        config['properties']['ConfigurationType'] = 'DynamicLibrary'
                    if label == 'missing':
                        config['items'].remove(item)
                    if label == 'metadata':
                        item['metadata'].pop(KEY)
                    if label == 'duplicate':
                        config['items'].append(copy.deepcopy(item))
                if label == 'configuration':
                    owner_item(data['Release'])['metadata'].pop(KEY)
                with self.assertRaises(ValueError):
                    translator.emit_platform_sources([], 'hw_engine' if label == 'owner' else 'launcher', data)

    def test_generated_selection_has_one_equal_configuration_rule(self):
        # Execute generated CMake with recording commands. This checks the evaluated
        # platform boundary without needing a cross compiler or asserting source text.
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            for owner in PROJECTS:
                lines = []
                translator.emit_platform_sources(lines, owner, configurations(owner))
                source = '\n'.join(lines)
                for platform, bits, expected in [('Darwin', 8, 1), ('Darwin', 4, 0),
                                                 ('Windows', 8, 0), ('Windows', 4, 0), ('Linux', 8, 0)]:
                    script = directory / 'selection.cmake'
                    receipt = directory / 'calls.txt'
                    receipt.write_text('')
                    script.write_text(f'''set(APPLE {int(platform == 'Darwin')})
set(WIN32 {int(platform == 'Windows')})
set(CMAKE_SYSTEM_NAME {platform})
set(CMAKE_SIZEOF_VOID_P {bits})
set(CSE_SDK_DIR "{translator.SDK}")
set(CSE_ROOT "{ROOT}")
function(target_sources)
endfunction()
function(set_property)
endfunction()
function(target_link_libraries)
endfunction()
function(cse_require_apple_main_image_publisher target requirement)
  file(APPEND "{receipt}" "${{target}}:${{requirement}}\\n")
endfunction()
''' + source)
                    subprocess.run(['cmake', '-P', str(script)], check=True, capture_output=True, text=True)
                    self.assertEqual(receipt.read_text().splitlines(), [f'{owner}:socket-v1'] * expected)


if __name__ == '__main__':
    unittest.main()
