#include "native_notifications.h"

#ifndef _WIN32
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <limits>
#include <new>
#include <pthread.h>

namespace {
using Status = A3dNativeWaitStatusV1;
using Clock = std::chrono::steady_clock;
class Event;
struct Waiter;

// One statically initialized normal mutex orders registry admission, state,
// reference ownership and all wait completions. Intrusive lists never allocate.
// There is no provider lock, virtual call, callback, or event-to-event lock order.
pthread_mutex_t ownerMutex = PTHREAD_MUTEX_INITIALIZER;
Event* events = nullptr;
Waiter* waiters = nullptr;
std::uint64_t nextGeneration = 0;
A3dNativeEventCounts counts{};
std::atomic<std::size_t> activeCalls{0};

class CallLease {
public:
    CallLease() noexcept {
        activeCalls.fetch_add(1);
    }
    ~CallLease() {
        activeCalls.fetch_sub(1);
    }
};

class Lock {
public:
    Lock() noexcept {
        pthread_mutex_lock(&ownerMutex);
    }
    ~Lock() {
        pthread_mutex_unlock(&ownerMutex);
    }
};

#ifdef A3D_EVENT_TESTING
A3dNativeEventHook testHook = nullptr;
void* testContext = nullptr;
std::atomic<A3dNativeEventFault> nextFault{A3dNativeEventFault::None};
pthread_cond_t testSleepingCondition = PTHREAD_COND_INITIALIZER;
std::size_t testSleeping = 0;
bool Fail(A3dNativeEventFault fault) noexcept {
    return nextFault.compare_exchange_strong(fault, A3dNativeEventFault::None);
}
#endif

class Event final : public IA3dNativeEventV1 {
public:
    explicit Event(const A3dNativeEventDescV1& desc) noexcept
        : manual(desc.resetMode == A3D_NATIVE_EVENT_MANUAL_RESET), signaled(desc.initiallySignaled != 0) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE Signal() override;
    HRESULT STDMETHODCALLTYPE Reset() override;
    HRESULT STDMETHODCALLTYPE Close() override;
    HRESULT STDMETHODCALLTYPE Wait(DWORD timeout, IA3dNativeEventV1* cancellation, Status* result) override;

    Event* next = nullptr;
    std::uint64_t generation = 0;
    ULONG references = 1;
    bool manual;
    bool signaled;
    bool closed = false;
};

struct Waiter {
    Event* target = nullptr;
    Event* cancellation = nullptr;
    Waiter* next = nullptr;
    pthread_cond_t condition;
    Clock::time_point deadline;
    bool infinite = false;
    Status status = Status::Invalid;
};

// Lookup compares full native addresses without dereferencing a supplied pointer.
// A live caller reference prevents reuse during admission. Retained internal
// tokens additionally verify generation, including after address reuse.
Event* Find(IA3dNativeEventV1* identity, std::uint64_t generation = 0) noexcept {
    for (Event* event = events; event; event = event->next) {
        if (static_cast<IA3dNativeEventV1*>(event) == identity &&
            (generation == 0 || event->generation == generation)) {
            return event;
        }
    }
    return nullptr;
}

// Locked decrement removes final ownership immediately. Destruction/free always
// runs unlocked while the enclosing API call still owns its module CallLease.
Event* Drop(Event* event) noexcept {
    if (!event || --event->references != 0) {
        return nullptr;
    }
    Event** link = &events;
    while (*link != event) {
        link = &(*link)->next;
    }
    *link = event->next;
    --counts.objects;
    return event;
}

void Destroy(Event* event) noexcept {
    if (event) {
        event->~Event();
        std::free(event);
    }
}

void Decide(Waiter& waiter) noexcept {
    if (waiter.status != Status::Invalid) {
        return;
    }
    if (waiter.target->closed) {
        waiter.status = Status::Closed;
    } else if (waiter.cancellation && (waiter.cancellation->closed || waiter.cancellation->signaled)) {
        waiter.status = Status::Cancelled;
    } else if (waiter.target->signaled) {
        waiter.status = Status::Signaled;
        if (!waiter.target->manual) {
            waiter.target->signaled = false;
        }
    } else if (!waiter.infinite && Clock::now() >= waiter.deadline) {
        waiter.status = Status::Timeout;
    }
}

void Resolve() noexcept {
    for (Waiter* waiter = waiters; waiter; waiter = waiter->next) {
        if (waiter->status == Status::Invalid) {
            Decide(*waiter);
            if (waiter->status != Status::Invalid) {
                // A committed result is persistent even if the waiter has not
                // reached pthread_cond_wait yet. Valid initialized CV signal
                // has no POSIX resource-failure path and never runs caller code.
                pthread_cond_signal(&waiter->condition);
            }
        }
    }
}

int InitCondition(pthread_cond_t* condition) noexcept {
#ifdef A3D_EVENT_TESTING
    if (Fail(A3dNativeEventFault::ConditionInit)) {
        return ENOMEM;
    }
#endif
#ifdef __APPLE__
    return pthread_cond_init(condition, nullptr);
#else
    pthread_condattr_t attributes;
    int error = pthread_condattr_init(&attributes);
    if (error != 0) {
        return error;
    }
    error = pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC);
    if (error == 0) {
        error = pthread_cond_init(condition, &attributes);
    }
    pthread_condattr_destroy(&attributes);
    return error;
#endif
}

int SleepCondition(Waiter& waiter) noexcept {
#ifdef A3D_EVENT_TESTING
    if (Fail(A3dNativeEventFault::ConditionWait)) {
        return EINVAL;
    }
#endif
    if (waiter.infinite) {
        return pthread_cond_wait(&waiter.condition, &ownerMutex);
    }
    const auto remaining = waiter.deadline - Clock::now();
    if (remaining <= Clock::duration::zero()) {
        return ETIMEDOUT;
    }
    constexpr std::int64_t kNanosecondsPerSecond = 1000000000;
    const auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(remaining).count();
    timespec timeout{static_cast<time_t>(nanoseconds / kNanosecondsPerSecond),
        static_cast<long>(nanoseconds % kNanosecondsPerSecond)};
#ifdef __APPLE__
    // Darwin's relative timed CV wait uses a monotonic interval. Recompute
    // remaining time after each real/spurious wake; never extend the deadline.
    return pthread_cond_timedwait_relative_np(&waiter.condition, &ownerMutex, &timeout);
#else
    timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return EINVAL;
    }
    timeout.tv_sec += now.tv_sec;
    timeout.tv_nsec += now.tv_nsec;
    if (timeout.tv_nsec >= kNanosecondsPerSecond) {
        ++timeout.tv_sec;
        timeout.tv_nsec -= kNanosecondsPerSecond;
    }
    return pthread_cond_timedwait(&waiter.condition, &ownerMutex, &timeout);
#endif
}

int Sleep(Waiter& waiter) noexcept {
#ifdef A3D_EVENT_TESTING
    ++testSleeping;
    pthread_cond_broadcast(&testSleepingCondition);
#endif
    const int error = SleepCondition(waiter);
#ifdef A3D_EVENT_TESTING
    --testSleeping;
    pthread_cond_broadcast(&testSleepingCondition);
#endif
    return error;
}

void* AllocateEvent() noexcept {
#ifdef A3D_EVENT_TESTING
    if (Fail(A3dNativeEventFault::Allocation)) {
        return nullptr;
    }
#endif
    return std::malloc(sizeof(Event));
}

HRESULT OperationError(int error) noexcept {
    return error == ENOMEM || error == EAGAIN ? E_OUTOFMEMORY : E_FAIL;
}

HRESULT Event::QueryInterface(REFIID iid, void** object) {
    CallLease call;
    if (!object) {
        return E_POINTER;
    }
    *object = nullptr;
    if (IsEqualIID(iid, IID_IUnknown) || IsEqualIID(iid, IID_IA3dNativeEventV1)) {
        Lock lock;
        ++references;
        *object = static_cast<IA3dNativeEventV1*>(this);
        return S_OK;
    }
    return E_NOINTERFACE;
}

ULONG Event::AddRef() {
    CallLease call;
    Lock lock;
    return ++references;
}

ULONG Event::Release() {
    CallLease call;
    Event* dead;
    ULONG remaining;
    {
        Lock lock;
        remaining = references - 1;
        dead = Drop(this);
    }
    Destroy(dead);
    return remaining;
}

HRESULT Event::Signal() {
    CallLease call;
    Lock lock;
    if (closed) {
        return E_ABORT;
    }
    signaled = true;
    Resolve();
    return S_OK;
}

HRESULT Event::Reset() {
    CallLease call;
    Lock lock;
    if (closed) {
        return E_ABORT;
    }
    signaled = false;
    return S_OK;
}

HRESULT Event::Close() {
    CallLease call;
    Lock lock;
    closed = true;
    signaled = false;
    Resolve();
    return S_OK;
}

HRESULT Event::Wait(DWORD timeout, IA3dNativeEventV1* cancellation, Status* result) {
    CallLease call;
    if (!result) {
        return E_POINTER;
    }
    *result = Status::Invalid;
    Waiter waiter;
    waiter.target = this;
    waiter.infinite = timeout == A3D_NATIVE_WAIT_INFINITE;
    waiter.deadline = Clock::now() + std::chrono::milliseconds(timeout);
    const int initialize = InitCondition(&waiter.condition);
    if (initialize != 0) {
        return OperationError(initialize);
    }
#ifdef A3D_EVENT_TESTING
    A3dNativeEventHook hook = nullptr;
    void* context = nullptr;
#endif
    bool admitted = false;
    {
        Lock lock;
        waiter.cancellation = cancellation ? Find(cancellation) : nullptr;
        if (!cancellation || (waiter.cancellation && waiter.cancellation != this && waiter.cancellation->manual)) {
            // Retention and linking happen under the same lock as all state
            // changes: cancellation/close cannot fall into an admission gap.
            ++references;
            if (waiter.cancellation) {
                ++waiter.cancellation->references;
            }
            waiter.next = waiters;
            waiters = &waiter;
            ++counts.waits;
            admitted = true;
            Decide(waiter);
#ifdef A3D_EVENT_TESTING
            if (waiter.status == Status::Invalid) {
                hook = testHook;
                context = testContext;
            }
#endif
        }
    }
    if (!admitted) {
        pthread_cond_destroy(&waiter.condition);
        return E_INVALIDARG;
    }
#ifdef A3D_EVENT_TESTING
    if (hook) {
        hook(context);
    }
#endif
    HRESULT operation = S_OK;
    Event* deadTarget;
    Event* deadCancellation;
    {
        Lock lock;
        while (waiter.status == Status::Invalid) {
            Decide(waiter);
            if (waiter.status != Status::Invalid) {
                break;
            }
            const int error = Sleep(waiter);
            if (error != 0 && error != ETIMEDOUT) {
                operation = OperationError(error);
                break;
            }
        }
        if (SUCCEEDED(operation)) {
            *result = waiter.status;
        }
        Waiter** link = &waiters;
        while (*link != &waiter) {
            link = &(*link)->next;
        }
        *link = waiter.next;
        --counts.waits;
        deadTarget = Drop(this);
        deadCancellation = Drop(waiter.cancellation);
    }
    pthread_cond_destroy(&waiter.condition);
    Destroy(deadCancellation);
    Destroy(deadTarget);
    return operation;
}
} // namespace

extern "C" HRESULT CSE_ABI_CALL CseCreateA3dNativeEventV1(
    const A3dNativeEventDescV1* description, IA3dNativeEventV1** event) {
    CallLease call;
    if (event) {
        *event = nullptr;
    }
    if (!description || !event) {
        return E_POINTER;
    }
    if (description->structureBytes != sizeof(*description) ||
        description->version != A3D_NATIVE_NOTIFICATION_VERSION ||
        description->resetMode > A3D_NATIVE_EVENT_MANUAL_RESET || description->initiallySignaled > 1) {
        return E_INVALIDARG;
    }
    void* memory = AllocateEvent();
    if (!memory) {
        return E_OUTOFMEMORY;
    }
    auto* created = new (memory) Event(*description);
    {
        Lock lock;
        // Never wrap a generation and accidentally re-admit a stale token.
        if (nextGeneration != std::numeric_limits<std::uint64_t>::max()) {
            created->generation = ++nextGeneration;
            created->next = events;
            events = created;
            ++counts.objects;
            *event = created;
        }
    }
    if (!*event) {
        Destroy(created);
        return E_FAIL;
    }
    return S_OK;
}

HRESULT A3dRetainNativeEvent(IA3dNativeEventV1* event, A3dNativeEventState* state) noexcept {
    CallLease call;
    if (!state) {
        return E_POINTER;
    }
    *state = {};
    Lock lock;
    Event* concrete = Find(event);
    if (!concrete) {
        return E_INVALIDARG;
    }
    ++concrete->references;
    ++counts.retainedStates;
    state->identity = concrete;
    state->generation = concrete->generation;
    return S_OK;
}

HRESULT A3dSignalNativeEvent(const A3dNativeEventState& state) noexcept {
    CallLease call;
    Lock lock;
    Event* event = state.generation ? Find(state.identity, state.generation) : nullptr;
    if (!event) {
        return E_INVALIDARG;
    }
    if (event->closed) {
        return E_ABORT;
    }
    event->signaled = true;
    Resolve();
    return S_OK;
}

void A3dReleaseNativeEvent(A3dNativeEventState* state) noexcept {
    CallLease call;
    if (!state) {
        return;
    }
    Event* dead = nullptr;
    {
        Lock lock;
        Event* event = state->generation ? Find(state->identity, state->generation) : nullptr;
        if (event) {
            --counts.retainedStates;
            dead = Drop(event);
        }
        *state = {};
    }
    Destroy(dead);
}

A3dNativeEventCounts A3dNativeEventLifetime() noexcept {
    Lock lock;
    auto snapshot = counts;
    snapshot.calls = activeCalls.load();
    return snapshot;
}

#ifdef A3D_EVENT_TESTING
void A3dNativeEventTestHook(A3dNativeEventHook hook, void* context) noexcept {
    Lock lock;
    testHook = hook;
    testContext = context;
}

bool A3dNativeEventTestAwaitSleeping(std::size_t expected, DWORD timeoutMilliseconds) noexcept {
    // Acquiring ownerMutex after Sleep increments testSleeping proves the CV
    // atomically released the mutex; the target wait has really blocked.
    Lock lock;
    const auto deadline = Clock::now() + std::chrono::milliseconds(timeoutMilliseconds);
    while (testSleeping != expected) {
        const auto remaining = deadline - Clock::now();
        if (remaining <= Clock::duration::zero()) {
            return false;
        }
        constexpr std::int64_t kNanosecondsPerSecond = 1000000000;
        const auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(remaining).count();
        timespec timeout{static_cast<time_t>(nanos / kNanosecondsPerSecond),
            static_cast<long>(nanos % kNanosecondsPerSecond)};
#ifdef __APPLE__
        const int error = pthread_cond_timedwait_relative_np(&testSleepingCondition, &ownerMutex, &timeout);
#else
        timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        timeout.tv_sec += now.tv_sec;
        timeout.tv_nsec += now.tv_nsec;
        if (timeout.tv_nsec >= kNanosecondsPerSecond) {
            ++timeout.tv_sec;
            timeout.tv_nsec -= kNanosecondsPerSecond;
        }
        const int error = pthread_cond_timedwait(&testSleepingCondition, &ownerMutex, &timeout);
#endif
        if (error != 0 && error != ETIMEDOUT) {
            return false;
        }
    }
    return true;
}

void A3dNativeEventTestFail(A3dNativeEventFault fault) noexcept {
    nextFault.store(fault);
}
#endif
#endif
