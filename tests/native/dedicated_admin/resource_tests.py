"""Validate CMake-selected runtime resources and, optionally, exact staged copies."""
import hashlib
import pathlib
import sys
root = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools'))
from cmake_inventory import query_target
from build_localization import runtime_files
selection = query_target(root, 'hlds', target_os='macos', architecture='arm64')['runtime_resources']
catalogue = root / 'src/deploy/localization'
translations = {str(path): data for path, data in runtime_files(catalogue)}
names = [pathlib.Path(item['source']).name for item in selection]
destinations = [item['destination'] for item in selection]
assert len(selection) == len(translations) + 15 and len(set(destinations)) == len(selection)
assert set(translations).issubset(destinations)
assert 'Admin.html' in names and 'CreateMultiplayerGameServerPage.res' in names
for item in selection:
    expected = pathlib.Path(item['source'])
    assert expected.is_file(), f'missing runtime resource: {expected}'
    if len(sys.argv) > 1:
        actual = pathlib.Path(sys.argv[1]) / item['destination']
        assert actual.is_file(), f'missing staged runtime resource: {actual}'
        data = translations.get(item['destination'], expected.read_bytes())
        assert hashlib.sha256(actual.read_bytes()).digest() == hashlib.sha256(data).digest(), expected.name
print(f'PASS {len(selection)} CMake-selected dedicated runtime resources'
      + (' and exact staged hashes' if len(sys.argv) > 1 else ''))
