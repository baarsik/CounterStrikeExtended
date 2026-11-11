"""Exercise the final-image gate with real linked images; never execute fixtures."""
import json
from pathlib import Path
import platform
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
CHECKER = ROOT / 'tools/main_image_publication.py'


@unittest.skipUnless(platform.system() == 'Darwin', 'Darwin linker controls')
class GateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory()
        cls.directory = Path(cls.temporary.name)
        cls.source = cls.directory / 'publication.c'
        cls.source.write_text('''#ifndef VISIBILITY
#define VISIBILITY "default"
#endif
#ifndef OMIT
__attribute__((visibility(VISIBILITY))) volatile const char CseNativeSocketHostV1[48] = {1};
#endif
__attribute__((visibility("default"))) void *CreateInterface(void) { return 0; }
int main(void) {
#ifndef OMIT
    return CseNativeSocketHostV1[0];
#else
    return 0;
#endif
}
''')
        cls.images = {}
        for label, flags in [('debug', ['-O0']), ('release', ['-O2', '-flto']),
                             ('hidden', ['-O2', '-flto', '-DVISIBILITY="hidden"']),
                             ('missing', ['-O2', '-flto', '-DOMIT']),
                             ('library', ['-dynamiclib']), ('x64', ['-arch', 'x86_64'])]:
            folder = cls.directory / label
            folder.mkdir()
            image = folder / 'hl'
            subprocess.run(['/usr/bin/clang', '-mmacosx-version-min=14.0', *flags,
                            str(cls.source), '-Wl,-export_dynamic', '-o', str(image)], check=True,
                           capture_output=True, text=True)
            cls.images[label] = image
        imported = cls.directory / 'imported.c'
        imported.write_text('extern const char CseNativeSocketHostV1[];\n'
                            'void *CreateInterface(void) { return 0; }\n'
                            'int main(void) { return CseNativeSocketHostV1[0]; }\n')
        folder = cls.directory / 'foreign'
        folder.mkdir()
        cls.images['foreign'] = folder / 'hl'
        subprocess.run(['/usr/bin/clang', '-mmacosx-version-min=14.0', str(imported),
                        '-Wl,-export_dynamic', '-Wl,-reexport_library,' + str(cls.images['library']),
                        '-o', str(cls.images['foreign'])], check=True, capture_output=True, text=True)

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def check(self, label, *, owner='launcher', architecture='arm64'):
        receipt = self.directory / 'receipt.json'
        receipt.unlink(missing_ok=True)
        result = subprocess.run([sys.executable, str(CHECKER), '--image', str(self.images[label]),
                                 '--owner', owner, '--architecture', architecture, '--receipt', str(receipt)],
                                capture_output=True, text=True)
        return result, json.loads(receipt.read_text()) if receipt.exists() else None

    def test_debug_and_lto_release_export_both_required_publications(self):
        for label in ('debug', 'release'):
            result, receipt = self.check(label)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(receipt['file_type'], 'MH_EXECUTE')
            self.assertIn('_CseNativeSocketHostV1', receipt['exports'])
            self.assertIn('_CreateInterface', receipt['exports'])

    def test_local_symbol_is_not_a_dynamic_publication(self):
        symbols = subprocess.check_output(['nm', '-a', str(self.images['hidden'])], text=True)
        self.assertIn('_CseNativeSocketHostV1', symbols)
        result, receipt = self.check('hidden')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Missing dynamic publication', result.stderr)
        self.assertIsNone(receipt)

    def test_dependency_publication_cannot_satisfy_the_main_image(self):
        symbols = subprocess.check_output(['nm', '-u', str(self.images['foreign'])], text=True)
        self.assertIn('_CseNativeSocketHostV1', symbols)
        result, receipt = self.check('foreign')
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Missing dynamic publication', result.stderr)
        self.assertIsNone(receipt)

    def test_missing_and_wrong_file_owner_and_architecture_reject(self):
        for label, options, diagnostic in [('missing', {}, 'Missing dynamic publication'),
                                          ('library', {}, 'MH_EXECUTE'),
                                          ('release', {'owner': 'hlds'}, 'owner output'),
                                          ('x64', {}, 'architecture')]:
            result, receipt = self.check(label, **options)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(diagnostic, result.stderr)
            self.assertIsNone(receipt)

    def test_owning_cmake_build_mandatorily_checks_final_images(self):
        for owner, output in [('launcher', 'hl'), ('hlds', 'hlds')]:
            for config in ('Debug', 'Release'):
                source_dir = self.directory / f'cmake-{owner}-{config}'
                source_dir.mkdir()
                (source_dir / 'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.25)
project(publication_control LANGUAGES C)
set(CSE_TARGET_ARCH arm64)
set(CMAKE_OSX_DEPLOYMENT_TARGET 14.0)
include("{ROOT}/cmake/MainImagePublication.cmake")
add_executable({owner} "{self.source}")
set_target_properties({owner} PROPERTIES OUTPUT_NAME {output})
target_compile_options({owner} PRIVATE "$<$<CONFIG:Release>:-flto>")
target_link_options({owner} PRIVATE "$<$<CONFIG:Release>:-flto>")
cse_require_apple_main_image_publisher({owner} socket-v1)
if(HIDDEN_PUBLICATION)
  target_compile_definitions({owner} PRIVATE VISIBILITY="hidden")
endif()
''')
                build = source_dir / 'build'
                configure = ['cmake', '-S', str(source_dir), '-B', str(build), '-G', 'Ninja',
                             '-DCMAKE_BUILD_TYPE=' + config, '-DCMAKE_OSX_ARCHITECTURES=arm64']
                subprocess.run(configure, check=True, capture_output=True, text=True)
                result = subprocess.run(['cmake', '--build', str(build), '--parallel', '1'],
                                        capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                receipt = build / 'main-image-publication' / config / (owner + '.json')
                self.assertEqual(json.loads(receipt.read_text())['owner'], owner)
                commands = subprocess.check_output(['ninja', '-C', str(build), '-t', 'commands', owner], text=True)
                link = next(line for line in commands.splitlines() if ' -o ' + output + ' ' in line)
                self.assertIn('-Wl,-export_dynamic', link)
                self.assertEqual('-flto' in link, config == 'Release')
                subprocess.run(configure + ['-DHIDDEN_PUBLICATION=ON'], check=True, capture_output=True, text=True)
                result = subprocess.run(['cmake', '--build', str(build), '--parallel', '1'],
                                        capture_output=True, text=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('Missing dynamic publication', result.stdout + result.stderr)
                self.assertFalse(receipt.exists())


if __name__ == '__main__':
    unittest.main()
