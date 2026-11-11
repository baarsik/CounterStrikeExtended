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
HRESULT A3dRetainNativeEvent(IA3dNativeEventV1* event, A3dNativeEventState* state, bool requireOpen = false) noexcept;
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

#include <array>
#include <atomic>
#include <mutex>

struct A3dNativeNotificationSet;

// A snapshot owns the immutable registration set until dispatch completes. Its
// fence is independent of the buffer/mixer lock. Dispatch invokes no COM virtual.
class A3dNativeNotificationBatch {
public:
    A3dNativeNotificationBatch() = default;
    ~A3dNativeNotificationBatch();
    A3dNativeNotificationBatch(const A3dNativeNotificationBatch&) = delete;
    A3dNativeNotificationBatch& operator=(const A3dNativeNotificationBatch&) = delete;
    void Dispatch() noexcept;
private:
    friend class A3dNativeNotifications;
    static constexpr std::size_t kBitsPerWord = 64;
    A3dNativeNotificationSet* set_ = nullptr;
    std::array<std::uint64_t, A3D_NATIVE_NOTIFICATION_MAX_POSITIONS / kBitsPerWord> matches_{};
};

class A3dNativeNotifications {
public:
    A3dNativeNotifications() = default;
    ~A3dNativeNotifications();
    A3dNativeNotifications(const A3dNativeNotifications&) = delete;
    A3dNativeNotifications& operator=(const A3dNativeNotifications&) = delete;
    void Shutdown() noexcept;
    HRESULT Replace(DWORD count, const A3dNativeNotificationPositionV1* positions, DWORD bytes);
    void Capture(DWORD oldPosition, std::uint64_t advance, DWORD bytes, bool stopped,
        A3dNativeNotificationBatch& batch) noexcept;
    static HRESULT Caps(A3dNativeNotificationCapsV1* caps, DWORD cursorKind) noexcept;
private:
    std::mutex mutex_;
    bool accepting_ = true;
    A3dNativeNotificationSet* set_ = nullptr;
};

#ifdef A3D_EVENT_TESTING
// Runs after capture, before the generation fence, without any owner lock.
void A3dNativeDispatchTestHook(A3dNativeEventHook hook, void* context) noexcept;
#endif
