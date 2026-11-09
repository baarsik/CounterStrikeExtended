#pragma once

#include <unknwn.h>

// Owned native contract version 1. UUIDv5 of the URL-namespace name
// "CounterStrikeExtended/IA3dNativeDeviceV1"; never the legacy IA3dDal IID.
constexpr IID IID_IA3dNativeDeviceV1 = {
    0x5951edd2, 0xe13f, 0x506b, {0xb3, 0x4e, 0xe0, 0x33, 0x6d, 0x86, 0x4d, 0xf8}
};

class IA3dNativeDeviceV1 : public IUnknown {
public:
    // Null selects the default device. The GUID is borrowed for this call only;
    // a provider that needs it afterward must copy its contents. No pointer is
    // packed into a DWORD, and initializationMode is a scalar, not a pointer.
    virtual HRESULT STDMETHODCALLTYPE InitializeDevice(const GUID* device, DWORD features,
        DWORD initializationMode, DWORD* supportedFeatures) = 0;
};
