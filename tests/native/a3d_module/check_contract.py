"""Check the actual declaration order, independent of the C++ method shape hash."""
from pathlib import Path
import re
import sys

root = Path(sys.argv[1])
sdk = root / 'GoldSrc-SDK/a3dapi'
public = (sdk / 'ia3dapi.h').read_text()
caller = (root / 'tests/native/a3d_module/caller_interfaces.h').read_text()
abi = (root / 'GoldSrc-SDK/public/cse_a3d_abi.h').read_text()
for target, view, file, parameter in [
    ('IA3dSource202', 'IA3dSource', 'A3dSource.h', 'Source'),
    ('IA3dGeomTarget', 'IA3dGeom', 'A3d3.h', 'Geometry'),
    ('IA3dListenerTarget', 'IA3dListener', 'A3d3.h', 'Listener'),
]:
    pattern = r'class ' + target + r'\s*:\s*public IUnknown\s*\{(.*?)\n\};'
    actual = re.search(pattern, (sdk / file).read_text(), re.S).group(1)
    consumer = re.search(pattern, caller, re.S).group(1)
    actual_names = ['QueryInterface', 'AddRef', 'Release'] + re.findall(
        r'virtual \w+ STDMETHODCALLTYPE (\w+)\(', actual)
    consumer_names = ['QueryInterface', 'AddRef', 'Release'] + re.findall(
        r'virtual \w+ STDMETHODCALLTYPE (\w+)\(', consumer)
    body = re.search(r'DECLARE_INTERFACE_\(' + view + r', IUnknown\)\s*\{(.*?)\n\};', public, re.S).group(1)
    public_names = [x or y for x, y in re.findall(r'STDMETHOD\((\w+)\)|STDMETHOD_\(\w+,\s*(\w+)\)', body)]
    if target == 'IA3dListenerTarget':
        public_names = public_names[:3] + ['Listener' + name for name in public_names[3:]]
    described = re.search(r'constexpr auto ' + target + r'Slots = Slots\(\{(.*?)\}\);', abi, re.S).group(1)
    described_names = re.findall(r'&' + parameter + r'::(\w+)\)', described)
    assert actual_names == consumer_names == public_names == described_names, target
    print(target, len(actual_names), 'exact ordered native slots; public and independent caller agree')
# The concrete list retains its destructor only after the complete public prefix.
list_header = (sdk / 'A3dList.h').read_text().split('class CA3dMaterial')[0]
assert list_header.index('EnableBoundingVol') < list_header.index('virtual ~CA3dList')
assert 'virtual ~' not in caller
