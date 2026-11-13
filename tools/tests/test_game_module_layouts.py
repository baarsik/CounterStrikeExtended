from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import game_module_layouts as layouts


class GameModuleLayoutTests(unittest.TestCase):
    def test_every_field_including_comma_members_and_nested_callback_is_retained(self):
        source = '''typedef struct table_s {
            int first, second;
            void (*callback)(int count, void (*visitor)(void *));
            char name[32];
        } Table;'''
        self.assertEqual(layouts.struct_fields(source, 'Table'), ['first', 'second', 'callback', 'name'])

    def test_unknown_member_syntax_fails_closed(self):
        with self.assertRaisesRegex(ValueError, 'Unsupported'):
            layouts.struct_fields('typedef struct { union { int value; }; } Table;', 'Table')

    def test_real_profile_has_game_and_save_contracts(self):
        root = Path(__file__).resolve().parents[2]
        result = layouts.generate(root)
        self.assertGreater(result['enginefuncs_t'], 150)
        self.assertGreater(result['DLL_FUNCTIONS'], 40)
        self.assertEqual(result['TYPEDESCRIPTION'], 8)
        text = (root / 'src/common/generated_game_module_layouts.h').read_text()
        self.assertIn('offsetof(TYPEDESCRIPTION, fieldOffset)', text)
        self.assertIn('alignof(decltype(((TYPEDESCRIPTION *)nullptr)->fieldOffset))', text)
        self.assertIn('offsetof(SAVERESTOREDATA, pTokens)', text)


if __name__ == '__main__':
    unittest.main()
