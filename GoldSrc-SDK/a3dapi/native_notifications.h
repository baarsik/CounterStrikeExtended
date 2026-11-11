#pragma once

#include "../public/cse_a3d_notifications.h"
#include <cstddef>

// Private module-local ownership token. A successful Retain owns one reference;
// transfer it without duplication, then Release exactly once. Copying its bytes
// does not retain. No caller virtual is invoked by these helpers. Provider locks
// may contain Retain/Signal/Release: this owner never calls back into a provider.
struct A3dNativeEventState {
    IA3dNativeEventV1* identity = nullptr;
    std::uint64_t generation = 0;
};
HRESULT A3dRetainNativeEvent(IA3dNativeEventV1* event, A3dNativeEventState* state) noexcept;
HRESULT A3dSignalNativeEvent(const A3dNativeEventState& state) noexcept;
void A3dReleaseNativeEvent(A3dNativeEventState* state) noexcept;

struct A3dNativeEventCounts {
    std::size_t objects;
    std::size_t retainedStates;
    std::size_t waits;
    std::size_t calls;
};
// Task 7 must combine this snapshot with its existing object/worker counts.
// The host holds its library lease through admission and all API calls. This is
// an unload eligibility observation, not protection from forced host dlclose.
A3dNativeEventCounts A3dNativeEventLifetime() noexcept;

#ifdef A3D_EVENT_TESTING
// Fixture-only scheduling and fault injection, absent from production builds.
// Admission callback runs unlocked after target/cancel retention and registration.
using A3dNativeEventHook = void (*)(void*);
void A3dNativeEventTestHook(A3dNativeEventHook hook, void* context) noexcept;
bool A3dNativeEventTestAwaitSleeping(std::size_t expected, DWORD timeoutMilliseconds) noexcept;
enum class A3dNativeEventFault { None, Allocation, ConditionInit, ConditionWait };
void A3dNativeEventTestFail(A3dNativeEventFault fault) noexcept;
#endif
