"""Keep product identities intact without requiring surrounding English prose."""
import json
from pathlib import Path
import re
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from build_localization import catalogue_tokens

SOURCE = ROOT / 'src/deploy/localization'
POLICY = json.loads(Path(__file__).with_name('brand_names.json').read_text(encoding='utf-8'))


class BrandNameTests(unittest.TestCase):
    def test_catalogue_product_identities(self):
        manifest = json.loads((SOURCE / 'manifest.json').read_text(encoding='utf-8'))
        checked = 0
        for entry in manifest['files']:
            if entry['kind'] != 'vgui':
                continue
            path = Path(entry['source'])
            family = str(path.parent / path.stem.rsplit('_', 1)[0])
            rules = POLICY['catalogues'].get(family, {})
            if not rules:
                continue
            tokens = catalogue_tokens((SOURCE / path).read_text(encoding='utf-8'))
            for token, required_groups in rules.items():
                if token not in tokens:
                    # Stock packs deliberately inherit some missing tokens from English.
                    self.assertNotEqual(entry['language'], 'english', family + '/' + token)
                    continue
                value = tokens[token].casefold()
                for alternatives in required_groups:
                    with self.subTest(file=entry['source'], token=token, alternatives=alternatives):
                        self.assertTrue(any(brand.casefold() in value for brand in alternatives), tokens[token])
                checked += 1
        self.assertGreater(checked, 300)

    def test_branded_anti_cheat_name_is_not_replaced_by_a_translation(self):
        manifest = json.loads((SOURCE / 'manifest.json').read_text(encoding='utf-8'))
        for entry in manifest['files']:
            if entry['kind'] != 'vgui':
                continue
            tokens = catalogue_tokens((SOURCE / entry['source']).read_text(encoding='utf-8'))
            for token, value in tokens.items():
                if token.startswith('[english]') or 'valve anti-cheat' in value.casefold():
                    # An accompanying explanation is valid if the proper name is retained.
                    continue
                for translated_name in POLICY['translated_technology_names']:
                    with self.subTest(file=entry['source'], token=token, translated_name=translated_name):
                        self.assertNotIn(translated_name.casefold(), value.casefold())

    def test_game_title_tokens_have_no_translated_aliases(self):
        for family, rules in POLICY['catalogues'].items():
            directory = SOURCE / Path(family).parent
            for path in sorted(directory.glob(Path(family).name + '_*.txt')):
                tokens = catalogue_tokens(path.read_text(encoding='utf-8'))
                for token, groups in rules.items():
                    if not any(brand in ('Counter-Strike', 'Counter-Strike: Condition Zero', 'Half-Life',
                                         'Condition Zero', 'Opposing Force', 'Team Fortress Classic',
                                         'Deathmatch Classic', 'Ricochet') for group in groups for brand in group):
                        continue
                    if token not in tokens:
                        continue
                    for alias in POLICY['translated_game_title_aliases']:
                        with self.subTest(file=path.name, token=token, alias=alias):
                            self.assertNotIn(alias.casefold(), tokens[token].casefold())
                for token, title in POLICY['pure_game_titles'].get(family, {}).items():
                    if token in tokens:
                        with self.subTest(file=path.name, token=token):
                            self.assertEqual(tokens[token].strip().casefold(), title.casefold())
        # The same Chinese words name the generic CT faction in these tokens.
        chinese = catalogue_tokens((SOURCE / 'cstrike/resource/cstrike_schinese.txt').read_text(encoding='utf-8'))
        self.assertEqual(chinese['Cstrike_ScoreBoard_CT'], '反恐精英')

    def test_game_selector_uses_original_titles(self):
        titles = POLICY['catalogues']['platform/Admin/server']
        for path in sorted((SOURCE / 'platform/Admin').glob('server_*.txt')):
            tokens = catalogue_tokens(path.read_text(encoding='utf-8'))
            for token in ('cstrike', 'valve', 'tfc', 'dmc', 'gearbox', 'ricochet'):
                with self.subTest(file=path.name, token=token):
                    self.assertEqual(tokens[token].casefold(), titles[token][0][0].casefold())

    def test_legacy_hud_titles_and_company_names(self):
        for family, tokens in POLICY['legacy'].items():
            for path in sorted(SOURCE.glob(family + '*/titles.txt')):
                text = path.read_text(encoding='utf-8')
                for token, required_groups in tokens.items():
                    match = re.search(r'(?m)^' + re.escape(token) + r'\s*\n\{([^}]*)}', text)
                    self.assertIsNotNone(match, str(path) + '/' + token)
                    for alternatives in required_groups:
                        with self.subTest(file=str(path.relative_to(SOURCE)), token=token):
                            self.assertTrue(any(brand.casefold() in match[1].casefold() for brand in alternatives),
                                            match[1].strip())


if __name__ == '__main__':
    unittest.main()
