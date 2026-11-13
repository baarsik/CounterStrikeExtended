"""Validate the maintained language pack against its supported interface scope."""
from collections import defaultdict
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from build_localization import catalogue_tokens, runtime_files


class MaintainedCatalogueTests(unittest.TestCase):
    def test_official_languages_cover_every_catalogue_and_legacy_hud(self):
        root = ROOT / 'src/deploy/localization'
        manifest = json.loads((root / 'manifest.json').read_text(encoding='utf-8'))
        languages = {'english', 'french', 'german', 'italian', 'spanish', 'schinese', 'tchinese', 'koreana'}
        self.assertEqual(set(manifest['languages']), languages)
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
