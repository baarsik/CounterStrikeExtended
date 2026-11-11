#include "A3d3.h"
#include "A3dSource.h"
#include "native_provider.h"
#include "native_notifications.h"
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include "resman.h"

#define CHECK(value) do { if (!(value)) { \
    std::fprintf(stderr, "line %d: %s\n", __LINE__, #value); std::exit(1); } } while (0)

// An active SDL callback can enter/leave while stable ownership is sampled.
// Final-release checks below still require every category, including Callback, to be zero.
A3dNativeOwnerCounts StableOwners() {
    auto counts = A3dNativeOwners();
    counts[static_cast<std::size_t>(A3dNativeOwnerKind::Callback)] = 0;
    return counts;
}

struct ReleaseBarrier {
    std::mutex mutex;
    std::condition_variable condition;
    bool reached = false;
    bool proceed = false;
    static void Pause(void* context) {
        auto& barrier = *static_cast<ReleaseBarrier*>(context);
        std::unique_lock<std::mutex> lock(barrier.mutex);
        if (barrier.reached) {
            return;
        }
        barrier.reached = true;
        barrier.condition.notify_all();
        CHECK(barrier.condition.wait_for(lock, std::chrono::seconds(5), [&] {
            return barrier.proceed;
        }));
    }
};

void ReleaseDuringShutdown() {
    IUnknown* provider = nullptr;
    CHECK(A3dCreateNativeProvider(&provider) == S_OK);
    IA3dNativeDeviceV1* device = nullptr;
    CHECK(provider->QueryInterface(IID_IA3dNativeDeviceV1, reinterpret_cast<void**>(&device)) == S_OK);
    DWORD supported = 0;
    CHECK(device->InitializeDevice(nullptr, 0, 1, &supported) == S_OK);
    auto* manager = dynamic_cast<ResMan*>(provider);
    CHECK(manager->SetResourceManagerMode(A3D_RESOURCE_MODE_DYNAMIC) == S_OK);
    WAVEFORMATEX format{WAVE_FORMAT_PCM, 1, 44100, 88200, 2, 16, 0};
    DSBUFFERDESC desc{};
    desc.dwSize = sizeof(desc);
    desc.dwBufferBytes = 16;
    desc.lpwfxFormat = &format;
    IDirectSoundBuffer* buffer = nullptr;
    CHECK(manager->CreateSoundBuffer(&desc, &buffer, nullptr) == S_OK);
    ReleaseBarrier barrier;
    A3dNativeSetFinalStreamReleaseHook(ReleaseBarrier::Pause, &barrier);
    std::thread releaser([&] {
        CHECK(buffer->Release() == 0);
    });
    {
        std::unique_lock<std::mutex> lock(barrier.mutex);
        CHECK(barrier.condition.wait_for(lock, std::chrono::seconds(5), [&] {
            return barrier.reached;
        }));
    }
    manager->ShutdownNative();
    {
        std::lock_guard<std::mutex> lock(barrier.mutex);
        barrier.proceed = true;
        barrier.condition.notify_all();
    }
    releaser.join();
    A3dNativeSetFinalStreamReleaseHook(nullptr, nullptr);
    device->Release();
    provider->Release();
    for (const auto count : A3dNativeOwners()) {
        CHECK(count == 0);
    }
}

int main(int argc, char** argv) {
    if (argc == 2 && !std::strcmp(argv[1], "release_during_shutdown")) {
        ReleaseDuringShutdown();
        std::puts("PASS release_during_shutdown");
        return 0;
    }
    const char* driver = std::getenv("SDL_AUDIO_DRIVER");
    CHECK(driver && !std::strcmp(driver, "dummy"));
    IUnknown* provider = nullptr;
    CHECK(A3dCreateNativeProvider(&provider) == S_OK);
    IA3dNativeDeviceV1* device = nullptr;
    CHECK(provider->QueryInterface(IID_IA3dNativeDeviceV1, reinterpret_cast<void**>(&device)) == S_OK);
    DWORD supported = 0;
    CHECK(device->InitializeDevice(nullptr, 0, 1, &supported) == S_OK);
    IDirectSound* sound = nullptr;
    IA3d2* resource = nullptr;
    CHECK(provider->QueryInterface(IID_IDirectSound, reinterpret_cast<void**>(&sound)) == S_OK);
    CHECK(provider->QueryInterface(IID_IA3d2, reinterpret_cast<void**>(&resource)) == S_OK);
    CHECK(resource->SetResourceManagerMode(A3D_RESOURCE_MODE_DYNAMIC) == S_OK);
    auto* parent = new CA3d(FALSE);
    CHECK(parent->InitializeInterfaces(sound, resource, nullptr) == S_OK);
    parent->AddRef();
    for (LONG failure = 0; failure < 3; ++failure) {
        const auto baseline = StableOwners();
        A3dNativeFailAcquisitionAfter(failure);
        LPA3DSOURCE rejected = reinterpret_cast<LPA3DSOURCE>(1);
        CHECK(FAILED(parent->NewSource(A3DSOURCE_TYPEDEFAULT, &rejected)) && !rejected);
        CHECK(StableOwners() == baseline);
        A3dNativeFailAcquisitionAfter(-1);
    }
    LPA3DSOURCE created = nullptr;
    CHECK(parent->NewSource(A3DSOURCE_TYPEDEFAULT, &created) == S_OK);
    auto* source = static_cast<CA3dSource*>(reinterpret_cast<IA3dSource202*>(created));
    IUnknown* sourceIdentity = nullptr;
    CHECK(source->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&sourceIdentity)) == S_OK);
    CHECK(source->AddRef() == 3);
    CHECK(source->Release() == 2);
    CHECK(sourceIdentity->Release() == 1);
    device->Release();
    provider->Release();
    WAVEFORMATEX format{WAVE_FORMAT_PCM, 1, 44100, 88200, 2, 16, 0};
    CHECK(source->SetWaveFormat(&format) == S_OK);
    CHECK(source->AllocateWaveData(16) == S_OK);
    for (LONG failure = 0; failure < 6; ++failure) {
        const auto baseline = StableOwners();
        A3dNativeFailAcquisitionAfter(failure);
        LPA3DSOURCE rejected = reinterpret_cast<LPA3DSOURCE>(1);
        CHECK(FAILED(parent->DuplicateSource(created, &rejected)) && !rejected);
        CHECK(StableOwners() == baseline);
        A3dNativeFailAcquisitionAfter(-1);
    }
    LPA3DSOURCE duplicateInterface = nullptr;
    CHECK(parent->DuplicateSource(created, &duplicateInterface) == S_OK);
    auto* duplicate = static_cast<CA3dSource*>(reinterpret_cast<IA3dSource202*>(duplicateInterface));
    CHECK(parent->Release() == 2);
    CHECK(duplicate->Release() == 0);
    IDirectSoundBuffer* buffer = source->m_pBuffer;
    buffer->AddRef();
    IA3dNativeNotifyV1* notify = nullptr;
    CHECK(buffer->QueryInterface(IID_IA3dNativeNotifyV1, reinterpret_cast<void**>(&notify)) == S_OK);
    A3dNativeEventDescV1 eventDescription{sizeof(eventDescription), 1, A3D_NATIVE_EVENT_AUTO_RESET, 0};
    IA3dNativeEventV1* event = nullptr;
    CHECK(CseCreateA3dNativeEventV1(&eventDescription, &event) == S_OK);
    A3dNativeNotificationPositionV1 position{0, 0, event};
    CHECK(notify->SetNotificationPositions(1, &position) == S_OK);
    CHECK(source->Release() == 0);
    std::thread waiter([&] {
        A3dNativeWaitStatusV1 status{};
        CHECK(event->Wait(2000, nullptr, &status) == S_OK && status == A3dNativeWaitStatusV1::Signaled);
        DWORD cursor = 0;
        CHECK(buffer->GetCurrentPosition(&cursor, nullptr) == S_OK);
        CHECK(notify->SetNotificationPositions(1, &position) == S_OK);
        CHECK(notify->SetNotificationPositions(0, nullptr) == S_OK);
        CHECK(buffer->Stop() == S_OK);
        CHECK(event->Close() == S_OK);
        notify->Release();
        CHECK(buffer->Release() == 0);
    });
    CHECK(A3dNativeEventTestAwaitSleeping(1, 2000));
    CHECK(buffer->Play(0, 0, DSBPLAY_LOOPING) == S_OK);
    waiter.join();
    event->Release();
    for (const auto owned : A3dNativeOwners()) {
        CHECK(owned == 0);
    }
    std::puts("PASS held_source_after_parent held_buffer_after_source");
}
