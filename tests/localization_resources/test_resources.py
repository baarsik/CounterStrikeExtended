"""Exercise generated runtime resources, including incremental build behavior."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
GENERATOR = ROOT / 'tools/build_localization.py'


class ResourceGenerationTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / 'sdk/deploy/localization'
        self.output = self.root / 'runtime'
        self.source.mkdir(parents=True)
        self.manifest = {'format_version': 1, 'files': [], 'languages': {'english': 'English'},
                         'native_language_names': {'english': 'English'}}

    def test_language_table_uses_manifest_order_and_unicode_names(self):
        self.manifest['languages']['polish'] = 'Polish'
        self.manifest['native_language_names']['polish'] = 'Polski'
        self.manifest['languages']['russian'] = 'Russian'
        self.manifest['native_language_names']['russian'] = 'Русский'
        header = self.root / 'include/languages.inc'
        result = self.run_generator('--language-header', str(header))
        self.assertEqual(result.returncode, 0, result.stderr)
        actual = header.read_text(encoding='ascii')
        self.assertIn('{"polish", L"Polski", "Polish"}', actual)
        self.assertIn('{"russian", L"\\u0420\\u0443\\u0441\\u0441\\u043a\\u0438\\u0439", "Russian"}', actual)
        self.assertLess(actual.index('"english"'), actual.index('"polish"'))
        self.assertLess(actual.index('"polish"'), actual.index('"russian"'))

    def test_language_table_rejects_missing_names_before_staging(self):
        self.add_file('some.txt', 'text', encoding='utf-8')
        self.manifest['languages']['polish'] = 'Polish'
        result = self.run_generator('--language-header', str(self.root / 'languages.inc'))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('native', result.stderr.lower())
        self.assertFalse(self.output.exists())

    def add_file(self, name, text, encoding='utf-16-le-bom', aliases=()):
        path = self.source / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding='utf-8')
        self.manifest['files'].append({'source': name, 'destination': name,
                                      'encoding': encoding, 'aliases': list(aliases),
                                      'kind': 'vgui' if encoding == 'utf-16-le-bom' else 'legacy'})

    def run_generator(self, *extra):
        (self.source / 'manifest.json').write_text(json.dumps(self.manifest), encoding='utf-8')
        return subprocess.run([sys.executable, str(GENERATOR), '--source', str(self.source),
                               '--output', str(self.output), *extra], capture_output=True, text=True)

    def test_unicode_and_format_arguments_survive_runtime_encoding(self):
        # Dropping the BOM, escaping text twice or encoding as ANSI breaks the game loader.
        text = '"lang"\n{\n"Language" "Korean"\n"Tokens"\n{\n"Example" "서버 %s1: 中文 &Oui\\n\\"quoted\\""\n}\n}\n'
        self.add_file('platform/resource/cse_koreana.txt', text)
        result = self.run_generator()
        self.assertEqual(result.returncode, 0, result.stderr)
        actual = (self.output / 'platform/resource/cse_koreana.txt').read_bytes()
        self.assertEqual(actual[:2], b'\xff\xfe')
        self.assertEqual(actual[2:].decode('utf-16-le'), text)

    def test_korean_alias_contains_identical_runtime_bytes(self):
        self.add_file('platform/resource/cse_koreana.txt', '"Tokens" { "Yes" "예" }\n',
                      aliases=['platform/resource/cse_korean.txt'])
        result = self.run_generator()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.output / 'platform/resource/cse_korean.txt').read_bytes(),
                         (self.output / 'platform/resource/cse_koreana.txt').read_bytes())

    def test_unchanged_files_are_preserved_and_deleted_files_are_restored(self):
        self.add_file('valve/resource/valve_english.txt', '"Tokens" { "Yes" "Yes" }\n')
        self.assertEqual(self.run_generator().returncode, 0)
        path = self.output / 'valve/resource/valve_english.txt'
        stamp = path.stat().st_mtime_ns
        self.assertEqual(self.run_generator().returncode, 0)
        self.assertEqual(path.stat().st_mtime_ns, stamp)
        path.unlink()
        self.assertEqual(self.run_generator().returncode, 0)
        self.assertEqual(path.read_bytes(), b'\xff\xfe' + '"Tokens" { "Yes" "Yes" }\n'.encode('utf-16-le'))

    def test_changed_source_updates_existing_output(self):
        self.add_file('platform/resource/cse_french.txt', '"Tokens" { "Yes" "Oui" }\n')
        self.assertEqual(self.run_generator().returncode, 0)
        (self.source / 'platform/resource/cse_french.txt').write_text('"Tokens" { "Yes" "D’accord" }\n', encoding='utf-8')
        result = self.run_generator()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('D’accord', (self.output / 'platform/resource/cse_french.txt').read_text(encoding='utf-16'))

    def test_legacy_text_preserves_declared_encoding(self):
        self.add_file('valve_french/titles.txt', 'Titre\n{\nÉnergie : %s\n}\n', encoding='cp1252')
        result = self.run_generator()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.output / 'valve_french/titles.txt').read_bytes(),
                         b'Titre\n{\n\xc9nergie : %s\n}\n')

    def test_emergency_fallbacks_are_generated_from_the_english_catalogue(self):
        self.add_file('platform/resource/cse_english.txt',
                      '"lang" {\n"Tokens" {\n"CSEServerBrowser_Example" "A \\"quote\\" and %s1"\n}\n}\n')
        header = self.root / 'include/english.inc'
        result = self.run_generator('--fallback-header', str(header))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('AddFallback("CSEServerBrowser_Example", L"A \\"quote\\" and %s1");',
                      header.read_text())
        source = self.source / 'platform/resource/cse_english.txt'
        source.write_text(source.read_text(encoding='utf-8').replace('and %s1', 'changed %s1'), encoding='utf-8')
        result = self.run_generator('--fallback-header', str(header))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('changed %s1', header.read_text())

    def test_missing_input_fails_without_partially_staging_files(self):
        self.add_file('platform/resource/cse_english.txt', '"Tokens" {}\n')
        self.manifest['files'].append({'source': 'missing.txt', 'destination': 'missing.txt',
                                      'encoding': 'utf-16-le-bom'})
        result = self.run_generator()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('missing.txt', result.stderr)
        self.assertFalse((self.output / 'platform/resource/cse_english.txt').exists())

    def test_malformed_token_file_is_rejected_before_staging(self):
        self.add_file('platform/resource/cse_english.txt', '"Tokens" { "Example" "unclosed }\n')
        result = self.run_generator()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('token', result.stderr.lower())
        self.assertFalse(self.output.exists())

    def test_project_translations_cannot_drop_substitution_arguments(self):
        self.add_file('platform/resource/cse_english.txt', '"Tokens" { "Example" "Player %s1" }\n')
        self.manifest['files'][-1].update(kind='project', language='english')
        self.add_file('platform/resource/cse_french.txt', '"Tokens" { "Example" "Joueur" }\n')
        self.manifest['files'][-1].update(kind='project', language='french')
        result = self.run_generator()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('argument', result.stderr.lower())

    def test_destination_escape_and_collisions_are_rejected(self):
        for destination in ('../escaped.txt', '/absolute.txt', 'C:/absolute.txt', '..\\escaped.txt'):
            with self.subTest(destination=destination):
                self.manifest['files'] = []
                self.add_file('valid.txt', 'text', encoding='utf-8')
                self.manifest['files'][0]['destination'] = destination
                result = self.run_generator()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('relative', result.stderr.lower())
        self.manifest['files'] = []
        self.add_file('one.txt', 'one', encoding='utf-8', aliases=['duplicate.txt'])
        self.add_file('two.txt', 'two', encoding='utf-8', aliases=['DUPLICATE.txt'])
        result = self.run_generator()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('duplicate', result.stderr.lower())

    def test_cmake_build_restores_resources_without_rebuilding_consumer(self):
        # A POST_BUILD-only copy would leave deleted resources missing on the second build.
        self.add_file('platform/resource/cse_french.txt', '"Tokens" { "Yes" "Oui" }\n')
        self.add_file('platform/resource/cse_english.txt',
                      '"Tokens" { "CSEServerBrowser_Example" "Example" }\n')
        (self.source / 'manifest.json').write_text(json.dumps(self.manifest), encoding='utf-8')
        project = self.root / 'project'
        project.mkdir()
        (project / 'CMakeLists.txt').write_text(f'''cmake_minimum_required(VERSION 3.25)
project(LocalizationFixture LANGUAGES NONE)
set(CSE_SDK_DIR "{(self.root / 'sdk').as_posix()}")
set(CSE_OUTPUT_DIRECTORY "{(self.root / 'custom runtime').as_posix()}/$<CONFIG>")
include("{(ROOT / 'cmake/Localization.cmake').as_posix()}")
add_custom_command(OUTPUT "${{CMAKE_BINARY_DIR}}/compiled.marker"
  COMMAND "${{CMAKE_COMMAND}}" -E touch "${{CMAKE_BINARY_DIR}}/compiled.marker"
  DEPENDS "${{CMAKE_BINARY_DIR}}/localization/include/ServerBrowserEnglish.generated.inc")
add_custom_target(client ALL DEPENDS "${{CMAKE_BINARY_DIR}}/compiled.marker")
cse_localization_assets(client)
''')
        for configuration in ('Debug', 'Release'):
            with self.subTest(configuration=configuration):
                build = self.root / ('build-' + configuration)
                result = subprocess.run(['cmake', '-S', str(project), '-B', str(build), '-G', 'Ninja',
                                         '-DCMAKE_BUILD_TYPE=' + configuration], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                command = ['cmake', '--build', str(build), '--target', 'client']
                result = subprocess.run(command, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                path = self.root / 'custom runtime' / configuration / 'platform/resource/cse_french.txt'
                self.assertIn('Oui', path.read_text(encoding='utf-16'))
                marker = build / 'compiled.marker'
                stamp = marker.stat().st_mtime_ns
                path.unlink()
                result = subprocess.run(command, capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertTrue(path.is_file())
                self.assertEqual(marker.stat().st_mtime_ns, stamp)


if __name__ == '__main__':
    unittest.main()
