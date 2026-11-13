"""Check the actual declaration order, independent of the C++ method shape hash."""
from pathlib import Path
import re
import sys

root = Path(sys.argv[1])
sdk = root / 'src/a3dapi'
public = (sdk / 'ia3dapi.h').read_text()
caller = (root / 'tests/native/a3d_module/caller_interfaces.h').read_text()
abi = (root / 'src/public/cse_a3d_abi.h').read_text()
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

# Public enumeration is independently declared and preserves the existing V1 view.
import uuid
assert str(uuid.uuid5(uuid.NAMESPACE_URL, 'CounterStrikeExtended/IA3dNativeDeviceEnumeratorV1')) == '7dcfb8c4-f874-5872-9c0f-d417ca72dd2a'
devices = root / 'src/public/cse_a3d_devices.h'
assert devices.exists(), 'absent public native enumeration declarations'
text = devices.read_text()
assert 'virtual ~' not in text
assert re.findall(r'virtual HRESULT STDMETHODCALLTYPE (\w+)\(', text) == ['GetCount', 'GetInfo']
assert 'CseCreateA3dNativeDeviceEnumeratorV1' in text
assert 'NativeDeviceEnumerationProfile = 1' in abi
old = (sdk / 'native_device_init.h').read_text()
assert re.findall(r'virtual HRESULT STDMETHODCALLTYPE (\w+)\(', old) == ['InitializeDevice']
assert 'MaxTables = 16' in (root / 'src/public/cse_module_abi.h').read_text()
print('enumeration UUID, public slots, old device slots and descriptor capacity agree')
import hashlib
assert hashlib.sha256((sdk / 'a3dapi.def').read_bytes()).hexdigest() == \
    'dd1735fadd19f5f22edba85a2f129f65ee1be1794f734f69b88f05d753ed99a1', 'Windows ordinals/exports changed'
independent = (root / 'tests/native/a3d_module/enumeration_caller.h').read_text()
exercise = (root / 'tests/native/a3d_module/enumeration_tests.cpp').read_text()
assert 'cse_a3d_devices.h' not in independent + exercise
assert 'cse_a3d_abi.h' not in independent + exercise
normalize = lambda value: re.sub(r'\s+', '', value)
required = '''virtual HRESULT STDMETHODCALLTYPE GetInfo(DWORD index, DWORD recordVersion, DWORD recordBytes,
    A3dNativeDeviceInfoV1* info, char* name, DWORD nameCapacity, DWORD* requiredNameBytes) = 0;'''
assert normalize(required) in normalize(text), 'GetInfo width/order differs from public contract'
assert normalize(required.replace('A3dNativeDeviceInfoV1', 'DeviceInfo')) in normalize(independent)
assert normalize('virtual HRESULT STDMETHODCALLTYPE GetCount(DWORD* count) = 0;') in normalize(text)
assert normalize('extern "C" HRESULT CSE_ABI_CALL CseCreateA3dNativeDeviceEnumeratorV1(\n'
                 'IA3dNativeDeviceEnumeratorV1** enumerator);') in normalize(text)
for field, offset, width in [('structureBytes', 0, 4), ('version', 4, 4), ('device', 8, 16),
                             ('nameBytes', 24, 4), ('reserved', 28, 4)]:
    assert f'CSE_A3D_DEVICE_FIELD({field}, {offset}, {width});' in text
assert 'sizeof(A3dNativeDeviceInfoV1) == 32 && alignof(A3dNativeDeviceInfoV1) == 4' in text
slots = re.search(r'constexpr auto IA3dNativeDeviceEnumeratorV1Slots = Slots\(\{(.*?)\}\);', abi, re.S).group(1)
assert re.findall(r'&IA3dNativeDeviceEnumeratorV1::(\w+)', slots) == [
    'QueryInterface', 'AddRef', 'Release', 'GetCount', 'GetInfo']
old_slots = re.search(r'constexpr auto IA3dNativeDeviceV1Slots = Slots\(\{(.*?)\}\);', abi, re.S).group(1)
assert re.findall(r'&IA3dNativeDeviceV1::(\w+)', old_slots) == [
    'QueryInterface', 'AddRef', 'Release', 'InitializeDevice']
print('exact public widths/order, five enumeration slots, four retained device slots, Windows export SHA unchanged')
