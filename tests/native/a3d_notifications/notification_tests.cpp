#include "native_buffer.h"
#include "native_notifications.h"
#include "native_output.h"
#ifdef A3D_STREAM_FIXTURE
#include "resman.h"
#include "dalinfo.h"
#endif
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <chrono>

#define CHECK(value) \
    do { \
        if (!(value)) { \
            std::fprintf(stderr, "line %d: %s\n", __LINE__, #value); \
            std::exit(1); \
        } \
    } while (0)

namespace {
constexpr DWORD kBytes = 16;
constexpr DWORD kWaitMilliseconds = 2000;

IA3dNativeEventV1* Event() {
    A3dNativeEventDescV1 desc{sizeof(desc), 1, A3D_NATIVE_EVENT_AUTO_RESET, 0};
    IA3dNativeEventV1* event = nullptr;
    CHECK(CseCreateA3dNativeEventV1(&desc, &event) == S_OK);
    return event;
}

bool Poll(IA3dNativeEventV1* event) {
    A3dNativeWaitStatusV1 result{};
    CHECK(event->Wait(0, nullptr, &result) == S_OK);
    return result == A3dNativeWaitStatusV1::Signaled;
}

struct Fixture {
    A3dNativeMixer mixer;
#ifdef A3D_STREAM_FIXTURE
    ResMan resourceOwner;
    ResManStreamBuffer streamBuffer{static_cast<IDirectSound*>(&resourceOwner), A3D_RESOURCE_MODE_DYNAMIC};
#endif
    IDirectSoundBuffer* buffer = nullptr;
    IA3dNativeNotifyV1* notify = nullptr;
    Fixture() {
        WAVEFORMATEX format{WAVE_FORMAT_PCM, 1, 22050, 44100, 2, 16, 0};
        DSBUFFERDESC desc{};
        desc.dwSize = sizeof(desc);
        desc.dwBufferBytes = kBytes;
        desc.lpwfxFormat = &format;
#ifdef A3D_STREAM_FIXTURE
        a3d_platform_buffer_desc_t streamDesc{sizeof(streamDesc), 0, kBytes, 0, &format};
        CHECK(streamBuffer.Init(&streamDesc, std::malloc(sizeof(DWORD))) == S_OK);
        buffer = static_cast<IDirectSoundBuffer*>(&streamBuffer);
        buffer->AddRef();
#else
        CHECK(mixer.CreateBuffer(&desc, &buffer) == S_OK);
#endif
        CHECK(buffer->QueryInterface(IID_IA3dNativeNotifyV1, reinterpret_cast<void**>(&notify)) == S_OK);
        A3dNativeNotificationCapsV1 caps{};
        caps.structureBytes = sizeof(caps);
        caps.version = 1;
        CHECK(notify->GetNotificationCaps(&caps) == S_OK);
        CHECK(caps.flags == 0x1f && caps.maximumPositions == 1024);
#ifdef A3D_STREAM_FIXTURE
        CHECK(caps.cursorKind == A3D_NATIVE_CURSOR_STREAM_SUBMITTED);
        CHECK(streamBuffer.SetNotificationPositions(1, static_cast<LPCDSBPOSITIONNOTIFY>(nullptr)) == E_NOTIMPL);
#else
        CHECK(caps.cursorKind == A3D_NATIVE_CURSOR_RENDER_CONSUMED);
#endif
        CHECK(caps.reserved[0] == 0 && caps.reserved[1] == 0 && caps.reserved[2] == 0);
        CHECK(notify->GetNotificationCaps(nullptr) == E_POINTER);
        caps.version = 2;
        CHECK(notify->GetNotificationCaps(&caps) == E_INVALIDARG);
        caps.version = 1;
        caps.structureBytes = 0;
        CHECK(notify->GetNotificationCaps(&caps) == E_INVALIDARG);
        void* legacy = reinterpret_cast<void*>(1);
        CHECK(buffer->QueryInterface(IID_IDirectSoundNotify, &legacy) == E_NOINTERFACE && !legacy);
        desc.dwFlags = DSBCAPS_CTRLPOSITIONNOTIFY;
        IDirectSoundBuffer* unsupported = reinterpret_cast<IDirectSoundBuffer*>(1);
        CHECK(mixer.CreateBuffer(&desc, &unsupported) == DSERR_UNSUPPORTED && !unsupported);
        IUnknown* first = nullptr;
        IUnknown* second = nullptr;
        CHECK(buffer->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&first)) == S_OK);
        CHECK(notify->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&second)) == S_OK);
        CHECK(first == second);
        first->Release();
        second->Release();
    }
    ~Fixture() {
        notify->Release();
        buffer->Release();
    }
    void Advance(DWORD bytes) {
        std::array<SHORT, 128> output{};
#ifdef A3D_STREAM_FIXTURE
        if (!bytes) {
            return;
        }
        WAVEFORMATEX format{WAVE_FORMAT_PCM, 1, 22050, 44100, 2, 16, 0};
        DSBUFFERDESC description{};
        description.dwSize = sizeof(description);
        description.dwBufferBytes = bytes;
        description.lpwfxFormat = &format;
        IDirectSoundBuffer* destination = nullptr;
        CHECK(mixer.CreateBuffer(&description, &destination) == S_OK);
        a3d_render_description_t streamDescription{A3D_RENDER_WRITE_BUFFER, bytes, 22050, 16, bytes};
        DalBufferInfo stream(nullptr, destination, &streamDescription, 2);
        CHECK(streamBuffer.SetStream(&stream) == S_OK);
        CHECK(streamBuffer.Update(0) == S_OK);
        DWORD status = 0;
        CHECK(buffer->GetStatus(&status) == S_OK);
        if (!(status & DSBSTATUS_LOOPING)) {
            CHECK(mixer.Render(output.data(), bytes / 2) == S_OK);
            CHECK(streamBuffer.Update(10) == S_OK);
        }
        CHECK(streamBuffer.SetStream(nullptr) == S_OK);
        destination->Release();
#else
        CHECK(mixer.Render(output.data(), bytes / 2) == S_OK);
#endif
    }
};

void Crossings() {
    Fixture f;
    const DWORD offsets[]{12, 15, 0, 3, 4, 11, A3D_NATIVE_NOTIFICATION_STOP};
    std::array<IA3dNativeEventV1*, 7> events{};
    std::array<A3dNativeNotificationPositionV1, 7> positions{};
    for (std::size_t i = 0; i < events.size(); ++i) {
        events[i] = Event();
        positions[i] = {offsets[i], 0, events[i]};
    }
    CHECK(f.notify->SetNotificationPositions(positions.size(), positions.data()) == S_OK);
    struct Case {
        DWORD old;
        DWORD advance;
        unsigned mask;
    };
    const Case cases[]{{12, 8, 0x0f}, {12, 4, 0x03}, {12, 32, 0x3f}, {0, 4, 0x0c}, {0, 0, 0}};
    for (const auto& test : cases) {
        CHECK(f.buffer->SetCurrentPosition(test.old) == S_OK);
        CHECK(f.buffer->Play(0, 0, DSBPLAY_LOOPING) == S_OK);
        f.Advance(test.advance);
        for (std::size_t i = 0; i < events.size(); ++i) {
            CHECK(Poll(events[i]) == ((test.mask & (1u << i)) != 0));
            CHECK(!Poll(events[i]));
        }
    }
    CHECK(f.buffer->Stop() == S_OK);
    CHECK(Poll(events.back()));
    CHECK(f.buffer->Stop() == S_OK);
    CHECK(!Poll(events.back()));
    CHECK(f.buffer->SetCurrentPosition(12) == S_OK);
    CHECK(f.buffer->Play(0, 0, 0) == S_OK);
    f.Advance(8);
    CHECK(Poll(events.back()));
    f.Advance(8);
    CHECK(!Poll(events.back()));
    CHECK(f.notify->SetNotificationPositions(0, nullptr) == S_OK);
    for (auto* event : events) {
        event->Close();
        event->Release();
    }
}

struct CommitOrder {
    std::array<DWORD, 3> offsets{};
    std::size_t count = 0;
    IA3dNativeEventV1* byteEvent = nullptr;
    bool byteVisibleAtStop = false;
    static void Observe(void* context, DWORD offset, IA3dNativeEventV1*) {
        auto& order = *static_cast<CommitOrder*>(context);
        CHECK(order.count < order.offsets.size());
        order.offsets[order.count++] = offset;
        if (offset == A3D_NATIVE_NOTIFICATION_STOP) {
            order.byteVisibleAtStop = Poll(order.byteEvent);
        }
    }
};

void StopAfterOffsets(bool sharedTarget) {
    Fixture f;
    auto* byteEvent = Event();
    auto* stopEvent = sharedTarget ? byteEvent : Event();
    auto* otherByte = Event();
    const A3dNativeNotificationPositionV1 positions[]{
        {A3D_NATIVE_NOTIFICATION_STOP, 0, stopEvent}, {15, 0, otherByte}, {12, 0, byteEvent}};
    CHECK(f.notify->SetNotificationPositions(3, positions) == S_OK);
    CHECK(f.buffer->SetCurrentPosition(12) == S_OK);
    CHECK(f.buffer->Play(0, 0, 0) == S_OK);
    CommitOrder order;
    order.byteEvent = byteEvent;
    A3dNativeCommitTestHook(CommitOrder::Observe, &order);
    f.Advance(8);
    A3dNativeCommitTestHook(nullptr, nullptr);
    CHECK(order.count == (sharedTarget ? 2 : 3));
    CHECK(order.offsets[0] == 15 && order.offsets[1] == 12);
    if (!sharedTarget) {
        CHECK(order.offsets[2] == A3D_NATIVE_NOTIFICATION_STOP);
        CHECK(order.byteVisibleAtStop);
    }
    CHECK(Poll(stopEvent));
    CHECK(!Poll(stopEvent));
    CHECK(Poll(otherByte));
    CHECK(f.notify->SetNotificationPositions(0, nullptr) == S_OK);
    if (!sharedTarget) {
        stopEvent->Close();
        stopEvent->Release();
    }
    byteEvent->Close();
    byteEvent->Release();
    otherByte->Close();
    otherByte->Release();
}

struct Barrier {
    std::mutex mutex;
    std::condition_variable condition;
    bool reached = false;
    bool released = false;
    static void Hook(void* context) {
        auto& barrier = *static_cast<Barrier*>(context);
        std::unique_lock<std::mutex> lock(barrier.mutex);
        barrier.reached = true;
        barrier.condition.notify_all();
        CHECK(barrier.condition.wait_for(lock, std::chrono::seconds(2), [&] { return barrier.released; }));
    }
    void Wait() {
        std::unique_lock<std::mutex> lock(mutex);
        CHECK(condition.wait_for(lock, std::chrono::seconds(2), [&] { return reached; }));
    }
    void Release() {
        std::lock_guard<std::mutex> lock(mutex);
        released = true;
        condition.notify_all();
    }
};

class ForeignEvent final : public IA3dNativeEventV1 {
public:
    unsigned calls = 0;
    HRESULT QueryInterface(REFIID, void**) override {
        ++calls;
        return E_FAIL;
    }
    ULONG AddRef() override {
        ++calls;
        return 1;
    }
    ULONG Release() override {
        ++calls;
        return 1;
    }
    HRESULT Signal() override {
        ++calls;
        return E_FAIL;
    }
    HRESULT Reset() override {
        ++calls;
        return E_FAIL;
    }
    HRESULT Close() override {
        ++calls;
        return E_FAIL;
    }
    HRESULT Wait(DWORD, IA3dNativeEventV1*, A3dNativeWaitStatusV1*) override {
        ++calls;
        return E_FAIL;
    }
};

void Fence(bool remove) {
    Fixture f;
    auto* old = Event();
    auto* replacement = Event();
    A3dNativeNotificationPositionV1 position{0, 0, old};
    CHECK(f.notify->SetNotificationPositions(1, &position) == S_OK);
    CHECK(f.buffer->Play(0, 0, DSBPLAY_LOOPING) == S_OK);
    Barrier barrier;
    A3dNativeDispatchTestHook(Barrier::Hook, &barrier);
    std::thread render([&] {
        f.Advance(4);
    });
    barrier.Wait();
    position.event = replacement;
    CHECK(f.notify->SetNotificationPositions(remove ? 0 : 1, &position) == S_OK);
    // The pending producer owns its old immutable snapshot. Replacement may
    // return immediately because dispatch has not crossed the commit fence.
    CHECK(!Poll(old));
    barrier.Release();
    render.join();
    A3dNativeDispatchTestHook(nullptr, nullptr);
    CHECK(!Poll(old));
    CHECK(!Poll(replacement));
    CHECK(f.buffer->SetCurrentPosition(0) == S_OK);
    f.Advance(4);
    CHECK(Poll(replacement) == !remove);
    old->Close();
    replacement->Close();
    old->Release();
    replacement->Release();
}

void Shutdown() {
    auto* event = Event();
    auto* mixer = new A3dNativeMixer;
    WAVEFORMATEX format{WAVE_FORMAT_PCM, 1, 22050, 44100, 2, 16, 0};
    DSBUFFERDESC desc{};
    desc.dwSize = sizeof(desc);
    desc.dwBufferBytes = kBytes;
    desc.lpwfxFormat = &format;
    IDirectSoundBuffer* buffer = nullptr;
    CHECK(mixer->CreateBuffer(&desc, &buffer) == S_OK);
    IA3dNativeNotifyV1* notify = nullptr;
    CHECK(buffer->QueryInterface(IID_IA3dNativeNotifyV1, reinterpret_cast<void**>(&notify)) == S_OK);
    A3dNativeNotificationPositionV1 position{A3D_NATIVE_NOTIFICATION_STOP, 0, event};
    CHECK(notify->SetNotificationPositions(1, &position) == S_OK);
    CHECK(buffer->Play(0, 0, DSBPLAY_LOOPING) == S_OK);
    delete mixer;
    CHECK(Poll(event));
    CHECK(A3dNativeEventLifetime().retainedStates == 0);
    CHECK(buffer->Stop() == S_OK);
    CHECK(!Poll(event));
    CHECK(buffer->Play(0, 0, 0) == E_ABORT);
    CHECK(notify->SetNotificationPositions(1, &position) == E_ABORT);
    notify->Release();
    buffer->Release();
    CHECK(event->Signal() == S_OK); // Provider shutdown never closes a caller event.
    event->Close();
    event->Release();
}

void Replace() {
    Fixture f;
    auto* event = Event();
    {
        A3dNativeNotificationPositionV1 position{0, 0, event};
        CHECK(f.notify->SetNotificationPositions(1, &position) == S_OK);
    }
    A3dNativeNotificationPositionV1 allocationPosition{4, 0, event};
    A3dNativeEventTestFail(A3dNativeEventFault::Allocation);
    CHECK(f.notify->SetNotificationPositions(1, &allocationPosition) == E_OUTOFMEMORY);
    A3dNativeEventTestFail(A3dNativeEventFault::None);
    CHECK(f.notify->SetNotificationPositions(1, nullptr) == E_POINTER);
    A3dNativeNotificationPositionV1 invalid{0, 1, event};
    CHECK(f.notify->SetNotificationPositions(1, &invalid) == E_INVALIDARG);
    CHECK(f.notify->SetNotificationPositions(1025, &invalid) == E_INVALIDARG);
    invalid = {16, 0, event};
    CHECK(f.notify->SetNotificationPositions(1, &invalid) == E_INVALIDARG);
    ForeignEvent foreign;
    invalid = {0, 0, &foreign};
    CHECK(f.notify->SetNotificationPositions(1, &invalid) == E_INVALIDARG);
    CHECK(foreign.calls == 0);
    auto* closed = Event();
    closed->Close();
    invalid = {0, 0, closed};
    CHECK(f.notify->SetNotificationPositions(1, &invalid) == E_ABORT);
    const auto beforeFailure = A3dNativeEventLifetime().retainedStates;
    std::array<A3dNativeNotificationPositionV1, 4> acquisitions{};
    for (std::size_t failure = 0; failure < acquisitions.size(); ++failure) {
        for (auto& entry : acquisitions) {
            entry = {0, 0, event};
        }
        acquisitions[failure].event = closed;
        CHECK(f.notify->SetNotificationPositions(acquisitions.size(), acquisitions.data()) == E_ABORT);
        CHECK(A3dNativeEventLifetime().retainedStates == beforeFailure);
    }
    closed->Release();
    CHECK(f.buffer->Play(0, 0, DSBPLAY_LOOPING) == S_OK);
    f.Advance(4);
    CHECK(Poll(event));
    CHECK(f.buffer->SetCurrentPosition(0) == S_OK);
    f.Advance(4);
    CHECK(f.notify->SetNotificationPositions(0, nullptr) == S_OK);
    CHECK(Poll(event)); // Already committed signal survives removal.
    CHECK(f.buffer->SetCurrentPosition(0) == S_OK);
    f.Advance(4);
    CHECK(!Poll(event));
    A3dNativeNotificationPositionV1 position{0, 0, event};
    CHECK(f.notify->SetNotificationPositions(1, &position) == S_OK);
    event->Release(); // Registration now owns the only event reference.
    CHECK(A3dNativeEventLifetime().objects == 1);
    CHECK(f.buffer->SetCurrentPosition(0) == S_OK);
    f.Advance(4);
    CHECK(f.notify->SetNotificationPositions(0, nullptr) == S_OK);
    CHECK(A3dNativeEventLifetime().objects == 0);
}

void DuplicateTargetCoalescing() {
    Fixture f;
    auto* event = Event();
    const A3dNativeNotificationPositionV1 positions[]{{0, 0, event}, {1, 0, event}};
    CHECK(f.notify->SetNotificationPositions(2, positions) == S_OK);
    std::array<A3dNativeWaitStatusV1, 2> results{};
    std::array<std::thread, 2> waiters;
    for (std::size_t i = 0; i < waiters.size(); ++i) {
        waiters[i] = std::thread([&, i] {
            CHECK(event->Wait(kWaitMilliseconds, nullptr, &results[i]) == S_OK);
        });
    }
    CHECK(A3dNativeEventTestAwaitSleeping(2, kWaitMilliseconds));
    CHECK(f.buffer->Play(0, 0, DSBPLAY_LOOPING) == S_OK);
    f.Advance(32);
    CHECK(event->Close() == S_OK);
    for (auto& waiter : waiters) {
        waiter.join();
    }
    const unsigned signaled = (results[0] == A3dNativeWaitStatusV1::Signaled) +
        (results[1] == A3dNativeWaitStatusV1::Signaled);
    CHECK(signaled == 1);
    CHECK(results[0] == A3dNativeWaitStatusV1::Closed || results[1] == A3dNativeWaitStatusV1::Closed);
    CHECK(f.notify->SetNotificationPositions(0, nullptr) == S_OK);
    event->Release();
}

void FinalRelease() {
    A3dNativeMixer mixer;
    auto* event = Event();
    WAVEFORMATEX format{WAVE_FORMAT_PCM, 1, 22050, 44100, 2, 16, 0};
    DSBUFFERDESC desc{};
    desc.dwSize = sizeof(desc);
    desc.dwBufferBytes = kBytes;
    desc.lpwfxFormat = &format;
    IDirectSoundBuffer* buffer = nullptr;
    CHECK(mixer.CreateBuffer(&desc, &buffer) == S_OK);
    IA3dNativeNotifyV1* notify = nullptr;
    CHECK(buffer->QueryInterface(IID_IA3dNativeNotifyV1, reinterpret_cast<void**>(&notify)) == S_OK);
    A3dNativeNotificationPositionV1 position{A3D_NATIVE_NOTIFICATION_STOP, 0, event};
    CHECK(notify->SetNotificationPositions(1, &position) == S_OK);
    notify->Release();
    CHECK(buffer->Play(0, 0, DSBPLAY_LOOPING) == S_OK);
    CHECK(buffer->Release() == 0);
    CHECK(Poll(event));
    CHECK(A3dNativeEventLifetime().retainedStates == 0);
    CHECK(event->Close() == S_OK);
    event->Release();
}

void CloseRace() {
    Fixture f;
    for (unsigned iteration = 0; iteration < 64; ++iteration) {
        auto* event = Event();
        A3dNativeNotificationPositionV1 position{0, 0, event};
        std::atomic<bool> begin{false};
        std::thread closer([&] {
            while (!begin.load()) {
                std::this_thread::yield();
            }
            CHECK(event->Close() == S_OK);
        });
        begin.store(true);
        const HRESULT status = f.notify->SetNotificationPositions(1, &position);
        CHECK(status == S_OK || status == E_ABORT);
        closer.join();
        CHECK(f.buffer->SetCurrentPosition(0) == S_OK);
        CHECK(f.buffer->Play(0, 0, DSBPLAY_LOOPING) == S_OK);
        f.Advance(4);
        A3dNativeWaitStatusV1 wait{};
        CHECK(event->Wait(0, nullptr, &wait) == S_OK && wait == A3dNativeWaitStatusV1::Closed);
        CHECK(f.notify->SetNotificationPositions(0, nullptr) == S_OK);
        CHECK(A3dNativeEventLifetime().retainedStates == 0);
        event->Release();
    }
}

void Reentry() {
    Fixture f;
    auto* event = Event();
    A3dNativeNotificationPositionV1 position{0, 0, event};
    CHECK(f.notify->SetNotificationPositions(1, &position) == S_OK);
    std::atomic<bool> complete{false};
    std::thread waiter([&] {
        A3dNativeWaitStatusV1 result{};
        CHECK(event->Wait(kWaitMilliseconds, nullptr, &result) == S_OK);
        CHECK(result == A3dNativeWaitStatusV1::Signaled);
        DWORD cursor = 0;
        CHECK(f.buffer->GetCurrentPosition(&cursor, nullptr) == S_OK);
        CHECK(f.notify->SetNotificationPositions(1, &position) == S_OK);
        CHECK(f.notify->SetNotificationPositions(0, nullptr) == S_OK);
        CHECK(f.buffer->Stop() == S_OK);
        CHECK(event->Close() == S_OK);
        complete.store(true);
    });
    CHECK(A3dNativeEventTestAwaitSleeping(1, kWaitMilliseconds));
    CHECK(f.buffer->Play(0, 0, DSBPLAY_LOOPING) == S_OK);
    f.Advance(4);
    waiter.join();
    CHECK(complete.load());
    event->Release();
}
void ReentryOutput() {
    const char* driver = std::getenv("SDL_AUDIO_DRIVER");
    CHECK(driver && !std::strcmp(driver, "dummy"));
    Fixture f;
    A3dNativeOutput output;
    auto* event = Event();
    A3dNativeNotificationPositionV1 position{0, 0, event};
    CHECK(f.notify->SetNotificationPositions(1, &position) == S_OK);
    std::thread waiter([&] {
        A3dNativeWaitStatusV1 result{};
        CHECK(event->Wait(kWaitMilliseconds, nullptr, &result) == S_OK);
        CHECK(result == A3dNativeWaitStatusV1::Signaled);
        DWORD cursor = 0;
        CHECK(f.buffer->GetCurrentPosition(&cursor, nullptr) == S_OK);
        CHECK(f.notify->SetNotificationPositions(1, &position) == S_OK);
        CHECK(f.buffer->Stop() == S_OK);
        CHECK(f.notify->SetNotificationPositions(0, nullptr) == S_OK);
        CHECK(event->Close() == S_OK);
    });
    CHECK(A3dNativeEventTestAwaitSleeping(1, kWaitMilliseconds));
    CHECK(f.buffer->Play(0, 0, DSBPLAY_LOOPING) == S_OK);
#ifdef A3D_STREAM_FIXTURE
    // Deterministic stream submission uses the real retained writer. The full
    // provider-worker/output variant is exercised in a3d_resources.
    f.Advance(4);
#else
    CHECK(output.Open(nullptr, f.mixer) == S_OK);
    CHECK(output.Start() == S_OK);
#endif
    waiter.join();
    output.Close();
    CHECK(output.Status() == S_OK);
    CHECK(A3dNativeEventLifetime().retainedStates == 0);
    event->Release();
}

}

int main(int argc, char** argv) {
    CHECK(argc == 2);
    if (!std::strcmp(argv[1], "notification_crossings")) {
        Crossings();
#ifndef A3D_STREAM_FIXTURE
        // Stream natural STOP is a later drain update, not the submission batch.
        StopAfterOffsets(false);
        StopAfterOffsets(true);
#endif
        DuplicateTargetCoalescing();
        Shutdown();
        FinalRelease();
    } else if (!std::strcmp(argv[1], "notification_replace_remove")) {
        Replace();
        Fence(false);
        Fence(true);
        CloseRace();
    } else if (!std::strcmp(argv[1], "notification_waiter_reentry")) {
        Reentry();
        ReentryOutput();
    } else {
        return 2;
    }
    for (const auto owned : A3dNativeOwners()) {
        CHECK(owned == 0);
    }
    const auto counts = A3dNativeEventLifetime();
    CHECK(!counts.objects && !counts.retainedStates && !counts.waits && !counts.calls);
    std::printf("PASS %s\n", argv[1]);
}
