"""Validate CMake-selected runtime resources and, optionally, exact staged copies."""
import hashlib
import pathlib
import sys
root = pathlib.Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'tools'))
from cmake_inventory import query_target
selection = query_target(root, 'hlds', target_os='macos', architecture='arm64')['runtime_resources']
names = [pathlib.Path(item['source']).name for item in selection]
assert len(names) == 16 and len(set(names)) == 16
assert 'Admin.html' in names and 'CreateMultiplayerGameServerPage.res' in names
for item in selection:
    expected = pathlib.Path(item['source'])
    assert expected.is_file(), f'missing runtime resource: {expected}'
    if len(sys.argv) > 1:
        actual = pathlib.Path(sys.argv[1]) / item['destination']
        assert actual.is_file(), f'missing staged runtime resource: {actual}'
        assert hashlib.sha256(actual.read_bytes()).digest() == hashlib.sha256(expected.read_bytes()).digest(), expected.name
print('PASS 16 CMake-selected dedicated runtime resources' + (' and exact staged hashes' if len(sys.argv) > 1 else ''))
