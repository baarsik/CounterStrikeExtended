#include "native_notifications.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#include <type_traits>

#define CHECK(value) \
    do { \
        if (!(value)) { \
            std::fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
            std::abort(); \
        } \
    } while (0)

using Status = A3dNativeWaitStatusV1;
constexpr auto kWatchdog = std::chrono::seconds(5);
constexpr DWORD kFiniteTimeoutMs = 15;
constexpr DWORD kWatchdogMilliseconds = 5000;

// This gate is invoked ONLY outside the production registry/event lock. It parks
// admitted waits before their first CV wait, forcing wake-before-sleep races.
struct Gate {
    std::mutex mutex;
    std::condition_variable condition;
    unsigned arrived = 0;
    bool released = false;

    static void Arrive(void* context) {
        auto& gate = *static_cast<Gate*>(context);
        std::unique_lock<std::mutex> lock(gate.mutex);
        ++gate.arrived;
        gate.condition.notify_all();
        CHECK(gate.condition.wait_for(lock, kWatchdog, [&] {
            return gate.released;
        }));
    }

    void Await(unsigned count) {
        std::unique_lock<std::mutex> lock(mutex);
        CHECK(condition.wait_for(lock, kWatchdog, [&] {
            return arrived >= count;
        }));
    }

    void Open() {
        std::lock_guard<std::mutex> lock(mutex);
        released = true;
        condition.notify_all();
    }
};

static IA3dNativeEventV1* Create(DWORD mode = A3D_NATIVE_EVENT_AUTO_RESET, bool signaled = false) {
    A3dNativeEventDescV1 desc{sizeof(desc), A3D_NATIVE_NOTIFICATION_VERSION, mode, signaled ? 1u : 0u};
    IA3dNativeEventV1* event = nullptr;
    CHECK(CseCreateA3dNativeEventV1(&desc, &event) == S_OK);
    CHECK(event);
    return event;
}

static Status Wait(IA3dNativeEventV1* event, DWORD timeout = 0, IA3dNativeEventV1* cancellation = nullptr) {
    Status result = Status::Invalid;
    CHECK(event->Wait(timeout, cancellation, &result) == S_OK);
    CHECK(result != Status::Invalid);
    return result;
}

static void Empty() {
    const auto counts = A3dNativeEventLifetime();
    CHECK(counts.objects == 0);
    CHECK(counts.retainedStates == 0);
    CHECK(counts.waits == 0);
    CHECK(counts.calls == 0);
}

struct Completions {
    std::mutex mutex;
    std::condition_variable condition;
    unsigned count = 0;
    void Done() {
        std::lock_guard<std::mutex> lock(mutex);
        ++count;
        condition.notify_all();
    }
    void Await(unsigned expected) {
        std::unique_lock<std::mutex> lock(mutex);
        CHECK(condition.wait_for(lock, kWatchdog, [&] {
            return count == expected;
        }));
    }
};

static void SleepingWaiters() {
    for (DWORD mode : {A3D_NATIVE_EVENT_AUTO_RESET, A3D_NATIVE_EVENT_MANUAL_RESET}) {
        auto* target = Create(mode);
        Completions completed;
        Status first = Status::Invalid;
        Status second = Status::Invalid;
        std::thread a([&] {
            first = Wait(target, A3D_NATIVE_WAIT_INFINITE);
            completed.Done();
        });
        std::thread b([&] {
            second = Wait(target, A3D_NATIVE_WAIT_INFINITE);
            completed.Done();
        });
        CHECK(A3dNativeEventTestAwaitSleeping(2, kWatchdogMilliseconds));
        CHECK(target->Signal() == S_OK);
        if (mode == A3D_NATIVE_EVENT_AUTO_RESET) {
            completed.Await(1);
            CHECK(A3dNativeEventTestAwaitSleeping(1, kWatchdogMilliseconds));
            CHECK(target->Close() == S_OK);
        }
        completed.Await(2);
        a.join();
        b.join();
        if (mode == A3D_NATIVE_EVENT_AUTO_RESET) {
            CHECK((first == Status::Signaled && second == Status::Closed) ||
                (second == Status::Signaled && first == Status::Closed));
        } else {
            CHECK(first == Status::Signaled && second == Status::Signaled);
            CHECK(Wait(target) == Status::Signaled);
        }
        target->Release();
    }
}

static void ResetWait() {
    SleepingWaiters();
    auto* event = Create(A3D_NATIVE_EVENT_AUTO_RESET, true);
    CHECK(Wait(event) == Status::Signaled);
    CHECK(Wait(event) == Status::Timeout);
    CHECK(event->Signal() == S_OK);
    CHECK(event->Signal() == S_OK);
    CHECK(Wait(event) == Status::Signaled);
    CHECK(Wait(event) == Status::Timeout);
    const auto start = std::chrono::steady_clock::now();
    CHECK(Wait(event, kFiniteTimeoutMs) == Status::Timeout);
    CHECK(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(kFiniteTimeoutMs));
    event->Release();

    for (DWORD mode : {A3D_NATIVE_EVENT_AUTO_RESET, A3D_NATIVE_EVENT_MANUAL_RESET}) {
        event = Create(mode);
        Gate gate;
        A3dNativeEventTestHook(Gate::Arrive, &gate);
        Status first = Status::Invalid;
        Status second = Status::Invalid;
        std::thread a([&] {
            first = Wait(event, A3D_NATIVE_WAIT_INFINITE);
        });
        std::thread b([&] {
            second = Wait(event, A3D_NATIVE_WAIT_INFINITE);
        });
        gate.Await(2);
        CHECK(A3dNativeEventLifetime().waits == 2);
        CHECK(event->Signal() == S_OK);
        // Reset after a committed Signal cannot retract the waiter result.
        CHECK(event->Reset() == S_OK);
        CHECK(event->Close() == S_OK);
        A3dNativeEventTestHook(nullptr, nullptr);
        gate.Open();
        a.join();
        b.join();
        if (mode == A3D_NATIVE_EVENT_AUTO_RESET) {
            CHECK((first == Status::Signaled && second == Status::Closed) ||
                (second == Status::Signaled && first == Status::Closed));
        } else {
            CHECK(first == Status::Signaled && second == Status::Signaled);
        }
        event->Release();
    }
    event = Create(A3D_NATIVE_EVENT_MANUAL_RESET);
    CHECK(event->Signal() == S_OK);
    CHECK(Wait(event) == Status::Signaled);
    CHECK(Wait(event) == Status::Signaled);
    CHECK(event->Reset() == S_OK);
    CHECK(Wait(event) == Status::Timeout);
    event->Release();
    Empty();
}

static void CancelClose() {
    for (bool closeCancellation : {false, true}) {
        auto* target = Create(A3D_NATIVE_EVENT_AUTO_RESET, true);
        auto* cancel = Create(A3D_NATIVE_EVENT_MANUAL_RESET);
        CHECK((closeCancellation ? cancel->Close() : cancel->Signal()) == S_OK);
        CHECK(Wait(target, 0, cancel) == Status::Cancelled);
        CHECK(Wait(target) == Status::Signaled); // Cancel does not consume target permit.
        CHECK(Wait(target, 0, cancel) == Status::Cancelled); // Sticky cancellation.
        CHECK(target->Close() == S_OK);
        CHECK(Wait(target, 0, cancel) == Status::Closed); // Target close has precedence.
        CHECK(target->Close() == S_OK);
        CHECK(target->Signal() == E_ABORT);
        CHECK(target->Reset() == E_ABORT);
        target->Release();
        cancel->Release();
    }
    // Actual CV wakeups, separately from pre-sleep admission races below.
    for (unsigned operation = 0; operation < 3; ++operation) {
        auto* target = Create();
        auto* cancel = Create(A3D_NATIVE_EVENT_MANUAL_RESET);
        Completions completed;
        Status result = Status::Invalid;
        std::thread waiter([&] {
            result = Wait(target, A3D_NATIVE_WAIT_INFINITE, cancel);
            completed.Done();
        });
        CHECK(A3dNativeEventTestAwaitSleeping(1, kWatchdogMilliseconds));
        if (operation == 0) {
            CHECK(target->Close() == S_OK);
        } else if (operation == 1) {
            CHECK(cancel->Close() == S_OK);
        } else {
            CHECK(cancel->Signal() == S_OK);
        }
        completed.Await(1);
        waiter.join();
        CHECK(result == (operation == 0 ? Status::Closed : Status::Cancelled));
        target->Release();
        cancel->Release();
    }
    // Fixed-seed choices exercise cancellation/target mutation across the exact
    // admission-before-sleep handshake, with no sleeps or scheduler guesses.
    constexpr std::uint32_t kRaceSeed = 0x6d2b79f5u;
    constexpr std::uint32_t kLcgMultiplier = 1664525u;
    constexpr std::uint32_t kLcgIncrement = 1013904223u;
    std::uint32_t random = kRaceSeed;
    constexpr unsigned kRaceIterations = 128;
    for (unsigned iteration = 0; iteration < kRaceIterations; ++iteration) {
        random = random * kLcgMultiplier + kLcgIncrement;
        const bool closeTarget = (random & 1u) != 0;
        const bool closeCancellation = (random & 2u) != 0;
        auto* target = Create();
        auto* cancel = Create(A3D_NATIVE_EVENT_MANUAL_RESET);
        Gate gate;
        A3dNativeEventTestHook(Gate::Arrive, &gate);
        Status result = Status::Invalid;
        std::thread waiter([&] {
            result = Wait(target, A3D_NATIVE_WAIT_INFINITE, cancel);
        });
        gate.Await(1);
        CHECK(A3dNativeEventLifetime().waits == 1);
        CHECK(A3dNativeEventLifetime().calls == 1);
        if (closeTarget) {
            CHECK(target->Close() == S_OK);
        } else {
            CHECK((closeCancellation ? cancel->Close() : cancel->Signal()) == S_OK);
        }
        // Caller drops cancellation only after admission has retained it.
        cancel->Release();
        CHECK(A3dNativeEventLifetime().objects == 2);
        if (closeTarget) {
            target->Release();
            CHECK(A3dNativeEventLifetime().objects == 2);
        }
        A3dNativeEventTestHook(nullptr, nullptr);
        gate.Open();
        waiter.join();
        CHECK(result == (closeTarget ? Status::Closed : Status::Cancelled));
        if (!closeTarget) {
            target->Release();
        }
        Empty();
    }
}

// Deliberately advertises the exact IID, but no method may be called to admit it.
struct ForeignEvent final : IA3dNativeEventV1 {
    unsigned callbacks = 0;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void** object) override {
        ++callbacks;
        *object = this;
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        ++callbacks;
        return 1;
    }
    ULONG STDMETHODCALLTYPE Release() override {
        ++callbacks;
        return 1;
    }
    HRESULT STDMETHODCALLTYPE Signal() override {
        ++callbacks;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Reset() override {
        ++callbacks;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Close() override {
        ++callbacks;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Wait(DWORD, IA3dNativeEventV1*, Status*) override {
        ++callbacks;
        return S_OK;
    }
};

static void Identity() {
    static_assert(sizeof(DWORD) == 4 && sizeof(HRESULT) == 4);
    static_assert(sizeof(Status) == 4);
    static_assert(static_cast<DWORD>(Status::Invalid) == 0);
    static_assert(static_cast<DWORD>(Status::Signaled) == 1);
    static_assert(static_cast<DWORD>(Status::Timeout) == 2);
    static_assert(static_cast<DWORD>(Status::Cancelled) == 3);
    static_assert(static_cast<DWORD>(Status::Closed) == 4);
    static_assert(A3D_NATIVE_NOTIFICATION_VERSION == 1);
    static_assert(A3D_NATIVE_EVENT_AUTO_RESET == 0 && A3D_NATIVE_EVENT_MANUAL_RESET == 1);
    static_assert(A3D_NATIVE_WAIT_INFINITE == 0xffffffffu && A3D_NATIVE_NOTIFICATION_STOP == 0xffffffffu);
    static_assert(A3D_NATIVE_NOTIFICATION_MAX_POSITIONS == 1024);
    static_assert(A3D_NATIVE_NOTIFY_BYTE_OFFSETS == 0x01 && A3D_NATIVE_NOTIFY_STOP == 0x02);
    static_assert(A3D_NATIVE_NOTIFY_WRAP == 0x04 && A3D_NATIVE_NOTIFY_ATOMIC_REPLACE == 0x08);
    static_assert(A3D_NATIVE_NOTIFY_RETAINED_EVENTS == 0x10);
    static_assert(A3D_NATIVE_CURSOR_RENDER_CONSUMED == 1 && A3D_NATIVE_CURSOR_STREAM_SUBMITTED == 2);
    using Factory = HRESULT (CSE_ABI_CALL *)(const A3dNativeEventDescV1*, IA3dNativeEventV1**);
    static_assert(std::is_same<decltype(&CseCreateA3dNativeEventV1), Factory>::value);
    static_assert(sizeof(A3dNativeEventDescV1) == 16);
    static_assert(offsetof(A3dNativeEventDescV1, version) == 4);
    static_assert(offsetof(A3dNativeEventDescV1, resetMode) == 8);
    static_assert(offsetof(A3dNativeEventDescV1, initiallySignaled) == 12);
    static_assert(sizeof(A3dNativeNotificationCapsV1) == 32);
    static_assert(offsetof(A3dNativeNotificationCapsV1, flags) == 8);
    static_assert(offsetof(A3dNativeNotificationCapsV1, maximumPositions) == 12);
    static_assert(offsetof(A3dNativeNotificationCapsV1, cursorKind) == 16);
    static_assert(offsetof(A3dNativeNotificationCapsV1, reserved) == 20);
    static_assert(sizeof(A3dNativeNotificationPositionV1) == 16);
    static_assert(offsetof(A3dNativeNotificationPositionV1, event) == 8);
    static_assert(!std::has_virtual_destructor<IA3dNativeEventV1>::value);
    static_assert(!std::has_virtual_destructor<IA3dNativeNotifyV1>::value);
    const IID eventIid{0xec05e92f, 0x4c19, 0x571e, {0x9e, 0xe4, 0xd1, 0x72, 0xa6, 0x1b, 0x0c, 0x3a}};
    const IID notifyIid{0x37efe390, 0x0647, 0x576f, {0x8f, 0x4d, 0xbb, 0xfe, 0x41, 0x5a, 0xcc, 0x81}};
    CHECK(IsEqualIID(eventIid, IID_IA3dNativeEventV1));
    CHECK(IsEqualIID(notifyIid, IID_IA3dNativeNotifyV1));
    A3dNativeEventDescV1 desc{sizeof(desc), 1, 0, 0};
    IA3dNativeEventV1* output = reinterpret_cast<IA3dNativeEventV1*>(1);
    CHECK(CseCreateA3dNativeEventV1(nullptr, &output) == E_POINTER && !output);
    CHECK(CseCreateA3dNativeEventV1(&desc, nullptr) == E_POINTER);
    for (unsigned field = 0; field < 4; ++field) {
        auto malformed = desc;
        if (field == 0) {
            malformed.structureBytes = 0;
        }
        if (field == 1) {
            malformed.version = 2;
        }
        if (field == 2) {
            malformed.resetMode = 2;
        }
        if (field == 3) {
            malformed.initiallySignaled = 2;
        }
        output = reinterpret_cast<IA3dNativeEventV1*>(1);
        CHECK(CseCreateA3dNativeEventV1(&malformed, &output) == E_INVALIDARG && !output);
    }
    A3dNativeEventTestFail(A3dNativeEventFault::Allocation);
    CHECK(CseCreateA3dNativeEventV1(&desc, &output) == E_OUTOFMEMORY && !output);
    Empty();
    auto* target = Create();
    CHECK(reinterpret_cast<std::uintptr_t>(target) > UINT32_MAX);
    void* unknown = nullptr;
    CHECK(target->QueryInterface(IID_IUnknown, &unknown) == S_OK);
    CHECK(unknown == target);
    void* eventView = nullptr;
    CHECK(static_cast<IUnknown*>(unknown)->QueryInterface(eventIid, &eventView) == S_OK);
    CHECK(eventView == target);
    static_cast<IUnknown*>(unknown)->Release();
    static_cast<IA3dNativeEventV1*>(eventView)->Release();
    CHECK(target->QueryInterface(eventIid, nullptr) == E_POINTER);
    unknown = reinterpret_cast<void*>(1);
    CHECK(target->QueryInterface(notifyIid, &unknown) == E_NOINTERFACE && !unknown);
    ForeignEvent foreign;
    auto* automatic = Create();
    for (auto* invalid : {target, automatic, static_cast<IA3dNativeEventV1*>(&foreign)}) {
        Status result = Status::Signaled;
        CHECK(target->Wait(0, invalid, &result) == E_INVALIDARG && result == Status::Invalid);
    }
    CHECK(foreign.callbacks == 0);
    CHECK(target->Wait(0, nullptr, nullptr) == E_POINTER);
    A3dNativeEventState retained;
    CHECK(A3dRetainNativeEvent(&foreign, &retained) == E_INVALIDARG);
    CHECK(foreign.callbacks == 0);
    CHECK(A3dRetainNativeEvent(target, &retained) == S_OK);
    CHECK(retained.generation != 0 && retained.identity == target);
    CHECK(A3dNativeEventLifetime().retainedStates == 1);
    automatic->Release();
    target->Release();
    CHECK(A3dNativeEventLifetime().objects == 1);
    CHECK(A3dSignalNativeEvent(retained) == S_OK);
    CHECK(Wait(retained.identity) == Status::Signaled);
    auto stale = retained;
    ++stale.generation;
    CHECK(A3dSignalNativeEvent(stale) == E_INVALIDARG);
    Status result = Status::Signaled;
    A3dNativeEventTestFail(A3dNativeEventFault::ConditionInit);
    CHECK(retained.identity->Wait(1, nullptr, &result) == E_OUTOFMEMORY && result == Status::Invalid);
    A3dNativeEventTestFail(A3dNativeEventFault::ConditionWait);
    CHECK(retained.identity->Wait(1, nullptr, &result) == E_FAIL && result == Status::Invalid);
    A3dReleaseNativeEvent(&retained);
    CHECK(!retained.identity && !retained.generation);
    CHECK(A3dSignalNativeEvent(stale) == E_INVALIDARG);
    Empty();
}

int main(int argc, char** argv) {
    CHECK(argc == 2);
    if (!std::strcmp(argv[1], "notification_event_reset_wait")) {
        ResetWait();
    } else if (!std::strcmp(argv[1], "notification_cancel_close")) {
        CancelClose();
    } else if (!std::strcmp(argv[1], "notification_event_identity")) {
        Identity();
    } else {
        CHECK(false);
    }
    std::puts("PASS");
}
