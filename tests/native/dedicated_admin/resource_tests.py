"""Validate required retained resources and, optionally, exact owning runtime copies."""
import hashlib
import pathlib
import sys
import xml.etree.ElementTree as ET
root = pathlib.Path(__file__).resolve().parents[3]
ns = {'m': 'http://schemas.microsoft.com/developer/msbuild/2003'}
project = ET.parse(root / 'GoldSrc-SDK/projects/vs2026/hlds.vcxproj')
selection = project.find('.//m:CseMacDedicatedResources', ns)
assert selection is not None, 'native dedicated runtime resource selection is missing'
names = selection.text.split(';')
source = root / 'GoldSrc-SDK/deploy/platform/Admin'
assert len(names) == 16 and len(set(names)) == 16
assert 'Admin.html' in names and 'CreateMultiplayerGameServerPage.res' in names
for name in names:
    expected = source / name
    assert expected.is_file(), f'missing runtime resource: {expected}'
    if len(sys.argv) > 1:
        actual = pathlib.Path(sys.argv[1]) / 'platform/Admin' / name
        assert actual.is_file(), f'missing staged runtime resource: {actual}'
        assert hashlib.sha256(actual.read_bytes()).digest() == hashlib.sha256(expected.read_bytes()).digest(), name
print('PASS 16 manifest-selected dedicated runtime resources' + (' and exact staged hashes' if len(sys.argv) > 1 else ''))
