#pragma once
#include "caller_interfaces.h"
#include "cse_module_abi.h"

// Deliberately independent declarations: this caller never includes the product device header.
namespace independent {
constexpr DWORD Version = 1;
constexpr IID EnumeratorIid = {
    0x7dcfb8c4, 0xf874, 0x5872, {0x9c, 0x0f, 0xd4, 0x17, 0xca, 0x72, 0xdd, 0x2a}
};
constexpr IID DeviceIid = {
    0x5951edd2, 0xe13f, 0x506b, {0xb3, 0x4e, 0xe0, 0x33, 0x6d, 0x86, 0x4d, 0xf8}
};
constexpr char FactoryName[] = "CseCreateA3dNativeDeviceEnumeratorV1";
struct DeviceInfo {
    DWORD structureBytes;
    DWORD version;
    GUID device;
    DWORD nameBytes;
    DWORD reserved;
};
struct Enumerator : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetCount(DWORD* count) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetInfo(DWORD index, DWORD recordVersion, DWORD recordBytes,
        DeviceInfo* info, char* name, DWORD nameCapacity, DWORD* requiredNameBytes) = 0;
};
struct Device : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE InitializeDevice(const GUID* device, DWORD features,
        DWORD initializationMode, DWORD* supportedFeatures) = 0;
};
using Factory = HRESULT (CSE_ABI_CALL *)(Enumerator**);
}
cse::abi::Descriptor ExpectedA3dEnumerationDescriptor();
bool CheckEnumerationAdmissionMutations(const cse::abi::Descriptor& offered);
