"""Production CMake selects the publisher only for its qualified executable owners."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools'))
from cmake_inventory import query_target

OWNERS = ('launcher', 'hlds')
PROPERTY = 'CSE_APPLE_MAIN_IMAGE_PUBLISHER'


class CMakeSelectionTests(unittest.TestCase):
    def test_production_publisher_selection_follows_platform_and_pointer_width(self):
        source = str(ROOT / 'src/launcher/native/SocketImageOwner.cpp')
        for owner in OWNERS:
            for configuration in ('Debug', 'Release'):
                for target_os, architecture, expected in (('macos', 'arm64', True), ('macos', 'x64', True),
                                                           ('macos', 'x86', False), ('windows', 'x86', False),
                                                           ('windows', 'x64', False), ('linux', 'x64', False)):
                    with self.subTest(owner=owner, configuration=configuration,
                                      target_os=target_os, architecture=architecture):
                        data = query_target(ROOT, owner, configuration, target_os, architecture)
                        self.assertEqual(data['properties'][PROPERTY], 'socket-v1' if expected else '')
                        self.assertEqual('-Wl,-export_dynamic' in data['link_options'], expected)
                        self.assertEqual([item['path'] for item in data['sources']
                                          if item['language']].count(source), int(expected))

    def test_real_helper_rejects_invalid_and_duplicate_requirements(self):
        cases = [('platform', False, 8, 'EXECUTABLE', 'launcher', 'socket-v1', 'arm64', False),
                 ('width', True, 4, 'EXECUTABLE', 'launcher', 'socket-v1', 'x86', False),
                 ('kind', True, 8, 'SHARED', 'launcher', 'socket-v1', 'arm64', False),
                 ('owner', True, 8, 'EXECUTABLE', 'hw_engine', 'socket-v1', 'arm64', False),
                 ('version', True, 8, 'EXECUTABLE', 'launcher', 'socket-v2', 'arm64', False),
                 ('architecture', True, 8, 'EXECUTABLE', 'launcher', 'socket-v1', 'x86', False),
                 ('duplicate', True, 8, 'EXECUTABLE', 'launcher', 'socket-v1', 'arm64', True)]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'stub.cpp').write_text('int main() { return 0; }\n')
            for label, apple, width, kind, owner, requirement, architecture, duplicate in cases:
                with self.subTest(case=label):
                    declaration = (f'add_executable({owner} stub.cpp)' if kind == 'EXECUTABLE'
                                   else f'add_library({owner} SHARED stub.cpp)')
                    call = f'cse_require_apple_main_image_publisher({owner} {requirement})\n'
                    (root / 'CMakeLists.txt').write_text(
                        'cmake_minimum_required(VERSION 3.25)\nproject(PublisherPolicy CXX)\n'
                        f'set(APPLE {int(apple)})\nset(CMAKE_SIZEOF_VOID_P {width})\n'
                        f'set(CSE_TARGET_ARCH {architecture})\n{declaration}\n'
                        f'include("{ROOT}/cmake/MainImagePublication.cmake")\n' + call + (call if duplicate else ''))
                    result = subprocess.run(['cmake', '-S', str(root), '-B', str(root / label), '-G', 'Ninja'],
                                            capture_output=True, text=True)
                    self.assertNotEqual(result.returncode, 0)
                    diagnostic = 'Duplicate Apple main-image publisher' if duplicate else (
                        'Invalid Apple main-image publisher owner/requirement')
                    self.assertIn(diagnostic, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
