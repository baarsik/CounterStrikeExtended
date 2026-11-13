"""The eager save catalog follows the owning manifests, not a second source list."""
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import saved_callback_catalog as catalog


class SavedCallbackCatalogTests(unittest.TestCase):
    def test_declaring_class_and_private_access(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            game = root / 'src/dlls'
            game.mkdir(parents=True)
            (game / 'cbase.h').write_text('class CBaseEntity { public: void Remove(); };\n')
            source = game / 'entity.cpp'
            source.write_text('''class Derived : public CBaseEntity {
                void Think();
                void Spawn() { SetThink(&Derived::Think); SetThink(&Derived::Remove); }
            };
            #include "generated_saved_callbacks/entity.inc"
            ''')
            result = catalog.generate(root, 'hl_server', [source])
            self.assertEqual(result['callbacks'], 2)
            generated = (game / 'generated_saved_callbacks/entity.inc').read_text()
            self.assertIn('Remove@CBaseEntity', generated)
            self.assertNotIn('Remove@Derived', generated)
            self.assertIn('CseSavedCallbackAccess<Derived>', generated)
            self.assertIn('static_cast<BASEPTR>(&Derived::Think)', generated)
            aggregate = (game / 'generated_saved_callbacks/catalog.inc').read_text()
            self.assertIn('CseRegisterCallbacks_entity()', aggregate)

    def test_assignment_guard_does_not_hide_declared_callback(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            game = root / 'src/dlls'
            game.mkdir(parents=True)
            (game / 'cbase.h').write_text('class Entity { public: void Think(); };')
            source = game / 'entity.cpp'
            source.write_text('#if FEATURE_A\nvoid Spawn() { SetThink(&Entity::Think); }\n#elif FEATURE_B\nvoid Other() { SetThink(&Entity::Think); }\n#endif\n#include "generated_saved_callbacks/entity.inc"\n')
            catalog.generate(root, 'hl_server', [source])
            generated = (game / 'generated_saved_callbacks/entity.inc').read_text()
            self.assertNotIn('FEATURE_A', generated)
            self.assertNotIn('FEATURE_B', generated)
            self.assertEqual(catalog.compile_guard(source.read_text(), source.read_text().index('void Other')),
                             '!(FEATURE_A) && (FEATURE_B)')

    def test_header_include_guard_does_not_disable_inline_callbacks(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            game = root / 'src/dlls'
            game.mkdir(parents=True)
            (game / 'effects.h').write_text('#ifndef EFFECTS_H\n#include \"skill.h\"\n#define EFFECTS_H\nclass Entity { public: void Think(); void Start() { SetThink(&Entity::Think); } };\n#endif\n')
            source = game / 'effects.cpp'
            source.write_text('#include "generated_saved_callbacks/effects.inc"')
            catalog.generate(root, 'hl_server', [source])
            generated = (game / 'generated_saved_callbacks/effects.inc').read_text()
            self.assertNotIn('defined(EFFECTS_H)', generated)
            self.assertIn('CseRegisterThink("Think@Entity"', generated)

    def test_conditional_method_declaration_is_preserved(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            game = root / 'src/dlls'
            game.mkdir(parents=True)
            (game / 'cbase.h').write_text('class Entity { public:\n#if FEATURE_A\nvoid Think();\n#endif\n};\n')
            source = game / 'entity.cpp'
            source.write_text('#if FEATURE_A\nvoid Spawn() { SetThink(&Entity::Think); }\n#endif\n#include "generated_saved_callbacks/entity.inc"\n')
            catalog.generate(root, 'hl_server', [source])
            generated = (game / 'generated_saved_callbacks/entity.inc').read_text()
            self.assertIn('#if FEATURE_A', generated)

    def test_unknown_or_virtual_assignment_is_rejected(self):
        for declaration, expression in [('void Think();', '&Missing::Think'),
                                        ('virtual void Think();', '&Entity::Think')]:
            with self.subTest(expression=expression), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                game = root / 'src/dlls'
                game.mkdir(parents=True)
                (game / 'cbase.h').write_text('class Entity { public: ' + declaration + ' };')
                source = game / 'entity.cpp'
                source.write_text('void Spawn() { SetThink(' + expression + '); }')
                with self.assertRaisesRegex(ValueError, 'Unresolved|virtual'):
                    catalog.generate(root, 'hl_server', [source])

    def test_excluded_sources_do_not_enter_catalog(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            game = root / 'src/dlls'
            game.mkdir(parents=True)
            (game / 'cbase.h').write_text('class Entity { public: void Think(); };')
            source = game / 'active.cpp'
            source.write_text('void Spawn() {}')
            (game / 'excluded.cpp').write_text('void Spawn() { SetThink(&Missing::Unknown); }')
            self.assertEqual(catalog.generate(root, 'hl_server', [source])['callbacks'], 0)


if __name__ == '__main__':
    unittest.main()
