#pragma once
#include "platform_com.h"

// UUIDv5(URL, "CounterStrikeExtended/IA3dNativeCapacityV1"). Software capacity
// is deliberately independent of vendor identity and public hardware caps.
constexpr IID IID_IA3dNativeCapacityV1 = {
    0x880f10d1, 0xc6b3, 0x5e59, {0xa0, 0x81, 0xf2, 0xf6, 0x34, 0xea, 0x9b, 0x49}
};
class IA3dNativeCapacityV1 : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE GetVoiceCapacity(DWORD* total, DWORD* available) = 0;
};
