"""Validate the maintained language pack against its supported interface scope."""
from collections import Counter, defaultdict
import json
from pathlib import Path
import sys
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from build_localization import catalogue_tokens, runtime_files


class MaintainedCatalogueTests(unittest.TestCase):
    def test_bundled_languages_cover_every_catalogue_and_legacy_hud(self):
        root = ROOT / 'src/deploy/localization'
        manifest = json.loads((root / 'manifest.json').read_text(encoding='utf-8'))
        languages = {'english', 'french', 'german', 'italian', 'spanish', 'schinese', 'tchinese', 'koreana', 'russian', 'polish'}
        self.assertEqual(set(manifest['languages']), languages)
        self.assertEqual(set(manifest['official_languages']), languages - {'russian', 'polish'})
        families = defaultdict(set)
        for entry in manifest['files']:
            if entry['kind'] in ('vgui', 'project'):
                family = Path(entry['source']).stem.rsplit('_', 1)[0]
                families[family].add(entry['language'])
            self.assertNotIn(Path(entry['source']).name, ('motd.txt', 'readme.txt'))
            self.assertFalse(Path(entry['source']).stem.endswith('_detail'))
            if entry['upstream']['kind'] != 'cse-original':
                self.assertEqual(len(entry['upstream']['sha256']), 64)
        self.assertEqual(set(families), {'admin', 'server', 'platform', 'vgui', 'serverbrowser',
                                        'valve', 'gameui', 'cstrike', 'cse'})
        for family, supplied in families.items():
            self.assertEqual(supplied, languages, family)
        outputs = {str(path): data for path, data in runtime_files(root)}
        for game in ('valve', 'cstrike'):
            for language in languages:
                directory = game if language == 'english' else game + '_' + language
                self.assertIn(directory + '/titles.txt', outputs)
        self.assertEqual(outputs['platform/resource/cse_korean.txt'],
                         outputs['platform/resource/cse_koreana.txt'])

    def test_added_languages_cover_english_tokens_arguments_and_legacy_files(self):
        root = ROOT / 'src/deploy/localization'
        manifest = json.loads((root / 'manifest.json').read_text(encoding='utf-8'))
        english = [entry for entry in manifest['files'] if entry['language'] == 'english']
        # Exclude a display percent before prose ("% complete") while retaining
        # numbered VGUI substitutions, printf arguments and named buddy tokens.
        arguments = re.compile(r'%(?:s\d+|[-+#0]*(?:\d+|\*)?(?:\.(?:\d+|\*))?'
                               r'(?:ll|l|h|z|I64)?[csdiuoxXfFeEgGaApn])(?![A-Za-z])|%[A-Za-z_]\w*%')
        for language in ('russian', 'polish'):
            entries = {entry['destination']: entry for entry in manifest['files']
                       if entry['language'] == language}
            self.assertTrue(entries, language)
            for base in english:
                path = Path(base['destination'])
                if base['kind'] == 'legacy':
                    destination = path.parts[0] + '_' + language + '/' + '/'.join(path.parts[1:])
                    self.assertIn(destination, entries)
                    continue
                destination = base['destination'].replace('_english.txt', '_' + language + '.txt')
                self.assertIn(destination, entries)
                tokens = catalogue_tokens((root / entries[destination]['source']).read_text(encoding='utf-8'))
                original = catalogue_tokens((root / base['source']).read_text(encoding='utf-8'))
                self.assertLessEqual(original.keys(), tokens.keys(), destination)
                for key, value in original.items():
                    self.assertEqual(Counter(arguments.findall(value)), Counter(arguments.findall(tokens[key])),
                                     language + '/' + key)

    def test_stock_fallback_record_matches_the_maintained_token_sets(self):
        root = ROOT / 'src/deploy/localization'
        manifest = json.loads((root / 'manifest.json').read_text(encoding='utf-8'))
        families = defaultdict(dict)
        for entry in manifest['files']:
            if entry['kind'] != 'vgui':
                continue
            family = Path(entry['source']).stem.rsplit('_', 1)[0]
            tokens = catalogue_tokens((root / entry['source']).read_text(encoding='utf-8'))
            families[family][entry['language']] = {key.casefold() for key in tokens
                                                 if not key.casefold().startswith('[english]')}
        observed = {}
        for family, versions in families.items():
            for language, tokens in versions.items():
                missing = sorted(versions['english'] - tokens)
                if missing:
                    observed.setdefault(family, {})[language] = missing
        self.assertEqual(observed, manifest['upstream_fallbacks'])


if __name__ == '__main__':
    unittest.main()
