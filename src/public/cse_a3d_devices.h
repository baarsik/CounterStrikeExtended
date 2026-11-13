#pragma once

#ifndef _WIN32
#include "../a3dapi/platform_com.h"
#include "cse_module_abi.h"
#include <type_traits>

constexpr DWORD A3D_NATIVE_DEVICE_ENUMERATION_VERSION = 1;
// UUIDv5 URL namespace, CounterStrikeExtended/IA3dNativeDeviceEnumeratorV1.
constexpr IID IID_IA3dNativeDeviceEnumeratorV1 = {
    0x7dcfb8c4, 0xf874, 0x5872, {0x9c, 0x0f, 0xd4, 0x17, 0xca, 0x72, 0xdd, 0x2a}
};
struct A3dNativeDeviceInfoV1 {
    DWORD structureBytes;
    DWORD version;
    GUID device;
    DWORD nameBytes;
    DWORD reserved;
};
static_assert(std::is_standard_layout_v<A3dNativeDeviceInfoV1>);
static_assert(sizeof(A3dNativeDeviceInfoV1) == 32 && alignof(A3dNativeDeviceInfoV1) == 4);
#define CSE_A3D_DEVICE_FIELD(field, offset, width) \
    static_assert(offsetof(A3dNativeDeviceInfoV1, field) == offset && \
        sizeof(decltype(A3dNativeDeviceInfoV1::field)) == width && \
        alignof(decltype(A3dNativeDeviceInfoV1::field)) == 4)
CSE_A3D_DEVICE_FIELD(structureBytes, 0, 4);
CSE_A3D_DEVICE_FIELD(version, 4, 4);
CSE_A3D_DEVICE_FIELD(device, 8, 16);
CSE_A3D_DEVICE_FIELD(nameBytes, 24, 4);
CSE_A3D_DEVICE_FIELD(reserved, 28, 4);
#undef CSE_A3D_DEVICE_FIELD

// Immutable snapshot. Each caller holds a reference and a host library lease. The host
// retains a main-thread anchor, joins readers, then performs terminal Release there.
// GetInfo clears at most min(recordBytes, 32) bytes; only version 1 / exactly 32 bytes
// is accepted. Null name with capacity zero queries size (S_FALSE); required includes
// NUL. Insufficient capacity returns no partial record/name. No module pointers escape.
struct IA3dNativeDeviceEnumeratorV1 : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetCount(DWORD* count) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetInfo(DWORD index, DWORD recordVersion, DWORD recordBytes,
        A3dNativeDeviceInfoV1* info, char* name, DWORD nameCapacity, DWORD* requiredNameBytes) = 0;
};
static_assert(!std::has_virtual_destructor_v<IA3dNativeDeviceEnumeratorV1>);

// Host must first establish SDL main-thread identity with SDL_SetMainReady on its
// actual main thread: an unregistered SDL_IsMainThread can report true on any thread.
// Construction/final Release and all host SDL init/quit transitions are serialized on
// that thread. Keep this snapshot until InitializeDevice returns; tokens are opaque,
// ephemeral, same-image/context only. Hotplug may still make initialization fail.
extern "C" HRESULT CSE_ABI_CALL CseCreateA3dNativeDeviceEnumeratorV1(
    IA3dNativeDeviceEnumeratorV1** enumerator);
#endif
