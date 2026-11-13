from pathlib import Path
root = Path(__file__).resolve().parents[3]
for name in ['VGui1NativeFontSelection.h', 'VGui1NativeFontSelection.cpp']:
    assert (root / 'src/engine' / name).is_file(), name
print('PASS new unselected source membership')
