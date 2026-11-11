#pragma once

#include "../a3dapi/platform_com.h"
#include "cse_module_abi.h"

constexpr DWORD A3D_NATIVE_NOTIFICATION_VERSION = 1;
constexpr DWORD A3D_NATIVE_EVENT_AUTO_RESET = 0;
constexpr DWORD A3D_NATIVE_EVENT_MANUAL_RESET = 1;
constexpr DWORD A3D_NATIVE_WAIT_INFINITE = 0xffffffffu;
constexpr DWORD A3D_NATIVE_NOTIFICATION_STOP = 0xffffffffu;
constexpr DWORD A3D_NATIVE_NOTIFICATION_MAX_POSITIONS = 1024;
constexpr DWORD A3D_NATIVE_NOTIFY_BYTE_OFFSETS = 0x01;
constexpr DWORD A3D_NATIVE_NOTIFY_STOP = 0x02;
constexpr DWORD A3D_NATIVE_NOTIFY_WRAP = 0x04;
constexpr DWORD A3D_NATIVE_NOTIFY_ATOMIC_REPLACE = 0x08;
constexpr DWORD A3D_NATIVE_NOTIFY_RETAINED_EVENTS = 0x10;
constexpr DWORD A3D_NATIVE_CURSOR_RENDER_CONSUMED = 1;
constexpr DWORD A3D_NATIVE_CURSOR_STREAM_SUBMITTED = 2;

// UUIDv5 URL namespace: CounterStrikeExtended/IA3dNativeEventV1 and
// CounterStrikeExtended/IA3dNativeNotifyV1. These are independent native views.
constexpr IID IID_IA3dNativeEventV1 = {
    0xec05e92f, 0x4c19, 0x571e, {0x9e, 0xe4, 0xd1, 0x72, 0xa6, 0x1b, 0x0c, 0x3a}
};
constexpr IID IID_IA3dNativeNotifyV1 = {
    0x37efe390, 0x0647, 0x576f, {0x8f, 0x4d, 0xbb, 0xfe, 0x41, 0x5a, 0xcc, 0x81}
};

enum class A3dNativeWaitStatusV1 : std::uint32_t {
    Invalid = 0,
    Signaled = 1,
    Timeout = 2,
    Cancelled = 3,
    Closed = 4
};

struct A3dNativeEventDescV1 {
    DWORD structureBytes;
    DWORD version;
    DWORD resetMode;
    DWORD initiallySignaled;
};

struct IA3dNativeEventV1 : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Signal() = 0;
    virtual HRESULT STDMETHODCALLTYPE Reset() = 0;
    virtual HRESULT STDMETHODCALLTYPE Close() = 0;
    virtual HRESULT STDMETHODCALLTYPE Wait(DWORD timeoutMilliseconds,
        IA3dNativeEventV1* cancellation, A3dNativeWaitStatusV1* result) = 0;
};

struct A3dNativeNotificationPositionV1 {
    DWORD offsetBytes;
    DWORD reserved;
    IA3dNativeEventV1* event;
};

struct A3dNativeNotificationCapsV1 {
    DWORD structureBytes;
    DWORD version;
    DWORD flags;
    DWORD maximumPositions;
    DWORD cursorKind;
    DWORD reserved[3];
};

struct IA3dNativeNotifyV1 : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetNotificationCaps(A3dNativeNotificationCapsV1* caps) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetNotificationPositions(DWORD count,
        const A3dNativeNotificationPositionV1* positions) = 0;
};

extern "C" HRESULT CSE_ABI_CALL CseCreateA3dNativeEventV1(
    const A3dNativeEventDescV1* description, IA3dNativeEventV1** event);
