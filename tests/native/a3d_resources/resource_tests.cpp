#include "native_buffer.h"
#include "native_notifications.h"
#include "resman.h"
#include "dalinfo.h"
#include "native_device_init.h"
#include "native_provider.h"
#include "native_capacity.h"
#include "native_output.h"
#include "a2dbuffer.h"
#include "emubuffer.h"
#include "rmstatbuffer.h"
#include "A3d3.h"
#include "a3dclsfc.h"
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <limits>

#define CHECK(value) do { if (!(value)) { \
    std::fprintf(stderr, "line %d: %s\n", __LINE__, #value); std::exit(1); } } while (0)

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

// A wrapped tail must consume all 80 source bytes, even when its last 16
// bytes were submitted at destination offset zero behind the current read cursor.
void WrappedStreamTail() {
    constexpr DWORD kSourceBytes = 80;
    constexpr DWORD kDestinationBytes = 64;
    constexpr DWORD kPooledStream = 2;
    A3dNativeMixer mixer;
    ResMan owner;
    ResManStreamBuffer source(static_cast<IDirectSound*>(&owner), A3D_RESOURCE_MODE_DYNAMIC);
    WAVEFORMATEX format{WAVE_FORMAT_PCM, 1, 22050, 44100, 2, 16, 0};
    a3d_platform_buffer_desc_t desc{sizeof(desc), 0, kSourceBytes, 0, &format};
    CHECK(source.Init(&desc, std::malloc(sizeof(DWORD))) == S_OK);
    DSBUFFERDESC outputDesc{};
    outputDesc.dwSize = sizeof(outputDesc);
    outputDesc.dwBufferBytes = kDestinationBytes;
    outputDesc.lpwfxFormat = &format;
    IDirectSoundBuffer* output = nullptr;
    CHECK(mixer.CreateBuffer(&outputDesc, &output) == S_OK);
    a3d_render_description_t streamDesc{A3D_RENDER_WRITE_BUFFER, kDestinationBytes, 22050, 16,
        kDestinationBytes};
    DalBufferInfo stream(nullptr, output, &streamDesc, kPooledStream);
    CHECK(source.SetStream(&stream) == S_OK);
    auto* stopped = Event();
    A3dNativeNotificationPositionV1 position{A3D_NATIVE_NOTIFICATION_STOP, 0, stopped};
    CHECK(source.SetNotificationPositions(1, &position) == S_OK);
    CHECK(source.Play(0, 0, 0) == S_OK);
    CHECK(source.Update(0) == S_OK);
    std::array<SHORT, 64> pcm{};
    CHECK(mixer.Render(pcm.data(), 24) == S_OK); // Consume 48 bytes; 16 remain before wrap.
    CHECK(source.Update(10) == S_OK); // Submit last 16 source bytes at destination zero.
    DWORD queued = 0;
    CHECK(stream.GetAudioBytesQueued(&queued) == S_OK && queued == 32);
    CHECK(mixer.Render(pcm.data(), 8) == S_OK);
    CHECK(source.Update(20) == S_OK);
    CHECK(!Poll(stopped)); // The physical ring boundary is not the source tail.
    CHECK(stream.GetAudioBytesQueued(&queued) == S_OK && queued == 16);
    CHECK(mixer.Render(pcm.data(), 8) == S_OK);
    CHECK(source.Update(30) == S_OK);
    CHECK(Poll(stopped));
    CHECK(static_cast<A3dNativeBuffer*>(output)->NativeConsumedBytes() == kSourceBytes);
    CHECK(source.Update(40) == S_OK && !Poll(stopped));

    // Reuse a stopped ring at byte 60. A first submission containing only the
    // source tail must also cross the physical boundary without an early STOP.
    CHECK(output->SetCurrentPosition(60) == S_OK);
    CHECK(source.SetCurrentPosition(64) == S_OK);
    CHECK(source.Play(0, 0, 0) == S_OK);
    CHECK(source.Update(50) == S_OK);
    CHECK(mixer.Render(pcm.data(), 2) == S_OK);
    CHECK(source.Update(60) == S_OK && !Poll(stopped));
    CHECK(stream.GetAudioBytesQueued(&queued) == S_OK && queued == 12);
    CHECK(mixer.Render(pcm.data(), 6) == S_OK);
    CHECK(source.Update(70) == S_OK && Poll(stopped));
    CHECK(static_cast<A3dNativeBuffer*>(output)->NativeConsumedBytes() == 96);
    CHECK(source.SetStream(nullptr) == S_OK);
    output->Release();
    stopped->Close();
    stopped->Release();
}

void FactoryAllocationFailure() {
    CA3dClassFactory factory;
    const auto baseline = A3dNativeOwners();
    A3dNativeFailAcquisitionAfter(0);
    IA3d3* object = reinterpret_cast<IA3d3*>(1);
    CHECK(factory.CreateInstance(nullptr, IID_IA3d3, reinterpret_cast<void**>(&object)) == E_OUTOFMEMORY);
    CHECK(!object);
    CHECK(A3dNativeOwners() == baseline);
    A3dNativeFailAcquisitionAfter(-1);
    CHECK(factory.CreateInstance(nullptr, IID_IA3d3, reinterpret_cast<void**>(&object)) == S_OK);
    object->Release();
}

void ConcurrentModes() {
    IUnknown* provider = nullptr;
    CHECK(A3dCreateNativeProvider(&provider) == S_OK);
    IA3dNativeDeviceV1* device = nullptr;
    CHECK(provider->QueryInterface(IID_IA3dNativeDeviceV1, reinterpret_cast<void**>(&device)) == S_OK);
    DWORD supported = 0;
    CHECK(device->InitializeDevice(nullptr, 0, 1, &supported) == S_OK);
    auto* manager = dynamic_cast<ResMan*>(provider);
    std::atomic<bool> start{false};
    constexpr DWORD kOperations = 1000;
    std::thread setter([&] {
        while (!start.load()) {
            std::this_thread::yield();
        }
        for (DWORD index = 0; index < kOperations; ++index) {
            CHECK(manager->SetResourceManagerMode(index % (A3D_RESOURCE_MODE_LAST + 1)) == S_OK);
        }
    });
    WAVEFORMATEX format{WAVE_FORMAT_PCM, 1, 44100, 88200, 2, 16, 0};
    DSBUFFERDESC desc{};
    desc.dwSize = sizeof(desc);
    desc.dwBufferBytes = kBytes;
    desc.lpwfxFormat = &format;
    start.store(true);
    for (DWORD index = 0; index < kOperations; ++index) {
        IDirectSoundBuffer* buffer = nullptr;
        CHECK(manager->CreateSoundBuffer(&desc, &buffer, nullptr) == S_OK);
        IA3dNativeNotifyV1* notify = nullptr;
        CHECK(buffer->QueryInterface(IID_IA3dNativeNotifyV1, reinterpret_cast<void**>(&notify)) == S_OK);
        A3dNativeNotificationCapsV1 caps{sizeof(caps), 1, 0, 0, 0, {}};
        CHECK(notify->GetNotificationCaps(&caps) == S_OK);
        CHECK(caps.cursorKind == A3D_NATIVE_CURSOR_RENDER_CONSUMED ||
            caps.cursorKind == A3D_NATIVE_CURSOR_STREAM_SUBMITTED);
        notify->Release();
        buffer->Release();
    }
    setter.join();
    device->Release();
    provider->Release();
}

void TypedProvider() {

    IUnknown* provider = nullptr;
    CHECK(A3dCreateNativeProvider(&provider) == S_OK && provider);
    IA3dNativeDeviceV1* device = nullptr;
    CHECK(provider->QueryInterface(IID_IA3dNativeDeviceV1, reinterpret_cast<void**>(&device)) == S_OK);
    IUnknown* identity = nullptr;
    CHECK(device->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&identity)) == S_OK);
    CHECK(identity == provider);
    identity->Release();
    DWORD supported = 123;
    CHECK(device->InitializeDevice(nullptr, A3D_1ST_REFLECTIONS, 1, &supported) == A3DERROR_FEATURE_NOT_SUPPORTED);
    CHECK(supported == 0);
    GUID invalid{0xdeadbeef, 0, 0, {}};
    CHECK(FAILED(device->InitializeDevice(&invalid, 0, 1, &supported)) && supported == 0);
    CHECK(device->InitializeDevice(nullptr, 0, 1, &supported) == S_OK && supported == A3D_DIRECT_PATH_A3D);
    IDirectSound* sound = nullptr;
    CHECK(provider->QueryInterface(IID_IDirectSound, reinterpret_cast<void**>(&sound)) == S_OK);
    DSCAPS caps{};
    caps.dwSize = sizeof(caps);
    CHECK(sound->GetCaps(&caps) == S_OK);
    CHECK(caps.dwMaxHwMixingAllBuffers == 0 && caps.dwFreeHw3DAllBuffers == 0);
    WAVEFORMATEX format{WAVE_FORMAT_PCM, 1, 22050, 44100, 2, 16, 0};
    DSBUFFERDESC desc{};
    desc.dwSize = sizeof(desc);
    desc.dwBufferBytes = kBytes;
    desc.lpwfxFormat = &format;
    IDirectSoundBuffer* buffer = nullptr;
    CHECK(sound->CreateSoundBuffer(&desc, &buffer, nullptr) == S_OK && buffer);
    sound->Release();
    device->Release();
    provider->Release();
    CHECK(buffer->Play(0, 0, DSBPLAY_LOOPING) == S_OK);
    buffer->Release();
}

void LegacyViews() {
    ResMan owner;
    ResManStatBuffer stat(static_cast<IDirectSound*>(&owner), A3D_RESOURCE_MODE_OFF);
    A2DBuffer a2d;
    EMUBuffer emu;
    IDirectSoundBuffer* buffers[]{&stat, &a2d, &emu};
    for (auto* buffer : buffers) {
        void* legacy = reinterpret_cast<void*>(1);
        CHECK(buffer->QueryInterface(IID_IDirectSoundNotify, &legacy) == E_NOINTERFACE && !legacy);
        void* typed = reinterpret_cast<void*>(1);
        CHECK(buffer->QueryInterface(IID_IA3dNativeNotifyV1, &typed) == E_NOINTERFACE && !typed);
    }
}

void Capacity() {
    CHECK(!IsEqualIID(IID_IA3dNativeCapacityV1, IID_IA3dNativeDeviceV1));
    IUnknown* leaf = nullptr;
    CHECK(A3dCreateNativeLeaf(&leaf) == S_OK);
    IA3dNativeDeviceV1* device = nullptr;
    CHECK(leaf->QueryInterface(IID_IA3dNativeDeviceV1, reinterpret_cast<void**>(&device)) == S_OK);
    DWORD supported = 0;
    CHECK(device->InitializeDevice(nullptr, 0, 1, &supported) == S_OK);
    IA3dNativeCapacityV1* capacity = nullptr;
    CHECK(leaf->QueryInterface(IID_IA3dNativeCapacityV1, reinterpret_cast<void**>(&capacity)) == S_OK);
    DWORD total = 0;
    DWORD available = 0;
    CHECK(capacity->GetVoiceCapacity(&total, &available) == S_OK && total == 8 && available == 8);
    IDirectSound* sound = nullptr;
    CHECK(leaf->QueryInterface(IID_IDirectSound, reinterpret_cast<void**>(&sound)) == S_OK);
    WAVEFORMATEX format{WAVE_FORMAT_PCM, 1, 44100, 88200, 2, 16, 0};
    DSBUFFERDESC desc{};
    desc.dwSize = sizeof(desc);
    desc.dwBufferBytes = kBytes;
    desc.lpwfxFormat = &format;
    std::array<IDirectSoundBuffer*, 8> buffers{};
    for (auto& buffer : buffers) {
        CHECK(sound->CreateSoundBuffer(&desc, &buffer, nullptr) == S_OK);
    }
    CHECK(capacity->GetVoiceCapacity(&total, &available) == S_OK && available == 0);
    IDirectSoundBuffer* rejected = reinterpret_cast<IDirectSoundBuffer*>(1);
    CHECK(sound->CreateSoundBuffer(&desc, &rejected, nullptr) == E_OUTOFMEMORY && !rejected);
    DSCAPS caps{};
    caps.dwSize = sizeof(caps);
    CHECK(sound->GetCaps(&caps) == S_OK && caps.dwFreeHw3DAllBuffers == 0 && caps.dwMaxHwMixingAllBuffers == 0);
    for (auto* buffer : buffers) {
        buffer->Release();
    }
    CHECK(capacity->GetVoiceCapacity(&total, &available) == S_OK && available == 8);
    constexpr std::size_t kContenders = 16;
    std::array<IDirectSoundBuffer*, kContenders> raced{};
    std::array<std::thread, kContenders> threads;
    std::atomic<unsigned> ready{0};
    std::atomic<bool> begin{false};
    for (std::size_t i = 0; i < kContenders; ++i) {
        threads[i] = std::thread([&, i] {
            ++ready;
            while (!begin.load()) {
                std::this_thread::yield();
            }
            const HRESULT result = sound->CreateSoundBuffer(&desc, &raced[i], nullptr);
            CHECK(result == S_OK || result == E_OUTOFMEMORY);
        });
    }
    while (ready.load() != kContenders) {
        std::this_thread::yield();
    }
    begin = true;
    for (auto& thread : threads) {
        thread.join();
    }
    unsigned created = 0;
    for (auto* buffer : raced) {
        if (buffer) {
            ++created;
            buffer->Release();
        }
    }
    CHECK(created == 8);
    sound->Release();
    capacity->Release();
    device->Release();
    leaf->Release();
}

void GuidCopy() {
    A3dNativeOutput tokens;
    GUID token{};
    CHECK(tokens.DeviceGuid(0, &token) == S_OK);
    IUnknown* provider = nullptr;
    CHECK(A3dCreateNativeProvider(&provider) == S_OK);
    IA3dNativeDeviceV1* device = nullptr;
    CHECK(provider->QueryInterface(IID_IA3dNativeDeviceV1, reinterpret_cast<void**>(&device)) == S_OK);
    DWORD supported = 0;
    CHECK(device->InitializeDevice(&token, 0, 1, &supported) == S_OK);
    std::memset(&token, 0xff, sizeof(token));
    auto* manager = dynamic_cast<ResMan*>(provider);
    CHECK(manager->SetResourceManagerMode(A3D_RESOURCE_MODE_DYNAMIC) == S_OK);
    CHECK(manager->TickNative(123) == S_OK);
    device->Release();
    provider->Release();
}

void NoFactoryRecursion() {
    CA3dClassFactory factory;
    IA3d3* object = nullptr;
    CHECK(factory.CreateInstance(nullptr, IID_IA3d3, reinterpret_cast<void**>(&object)) == S_OK);
    CHECK(object->Init(nullptr, 0, 0) == S_OK);
    object->Release();
}

void StreamSplitProgress() {
    A3dNativeMixer mixer;
    ResMan owner;
    ResManStreamBuffer source(static_cast<IDirectSound*>(&owner), A3D_RESOURCE_MODE_DYNAMIC);
    WAVEFORMATEX format{WAVE_FORMAT_PCM, 1, 22050, 44100, 2, 16, 0};
    a3d_platform_buffer_desc_t desc{sizeof(desc), 0, kBytes, 0, &format};
    CHECK(source.Init(&desc, std::malloc(sizeof(DWORD))) == S_OK);
    DSBUFFERDESC outputDesc{};
    outputDesc.dwSize = sizeof(outputDesc);
    outputDesc.dwBufferBytes = 64;
    outputDesc.lpwfxFormat = &format;
    IDirectSoundBuffer* output = nullptr;
    CHECK(mixer.CreateBuffer(&outputDesc, &output) == S_OK);
    a3d_render_description_t streamDesc{A3D_RENDER_WRITE_BUFFER, 64, 22050, 16, 64};
    DalBufferInfo stream(nullptr, output, &streamDesc, 2);
    CHECK(source.SetStream(&stream) == S_OK);
    CHECK(source.SetCurrentPosition(2) == S_OK);
    CHECK(source.Play(0, 0, DSBPLAY_LOOPING) == S_OK);
    CHECK(source.Update(0) == S_OK);
    CHECK(stream.m_nativeSubmittedBytes == 64);
    std::array<SHORT, 64> pcm{};
    CHECK(mixer.Render(pcm.data(), 24) == S_OK);
    CHECK(source.Update(1) == S_OK);
    CHECK(stream.m_nativeSubmittedBytes == 48 && stream.m_dwWritePosition == 48);
    CHECK(mixer.Render(pcm.data(), 16) == S_OK);
    CHECK(source.Update(2) == S_OK);
    CHECK(stream.m_nativeSubmittedBytes == 32);
    CHECK(source.Update(1) == S_OK);
    CHECK(stream.m_nativeSubmittedBytes == 0);
    DWORD position = 0;
    CHECK(source.GetCurrentPosition(&position, nullptr) == S_OK && position == 2);
    stream.m_dwUnusedSince = 0xfffffff0u;
    stream.m_dwTimeout = 10000;
    CHECK(!stream.IsExpired(0x00002700u));
    CHECK(stream.IsExpired(0x00002710u));
    CHECK(source.SetStream(nullptr) == S_OK);
    output->Release();
}

void InitFailReinit() {
    constexpr LONG kAcquisitions = 10;
    for (LONG failure = 0; failure < kAcquisitions; ++failure) {
        IUnknown* provider = nullptr;
        CHECK(A3dCreateNativeProvider(&provider) == S_OK);
        IA3dNativeDeviceV1* device = nullptr;
        CHECK(provider->QueryInterface(IID_IA3dNativeDeviceV1, reinterpret_cast<void**>(&device)) == S_OK);
        const auto baseline = A3dNativeOwners();
        A3dNativeFailAcquisitionAfter(failure);
        DWORD supported = 123;
        CHECK(FAILED(device->InitializeDevice(nullptr, 0, 1, &supported)));
        CHECK(supported == 0);
        CHECK(A3dNativeOwners() == baseline);
        A3dNativeFailAcquisitionAfter(-1);
        CHECK(device->InitializeDevice(nullptr, 0, 1, &supported) == S_OK);
        device->Release();
        provider->Release();
        for (const auto count : A3dNativeOwners()) {
            CHECK(count == 0);
        }
    }
}

void PoolFailures() {
    for (LONG failure = 0; failure < 3; ++failure) {
        IUnknown* provider = nullptr;
        CHECK(A3dCreateNativeProvider(&provider) == S_OK);
        IA3dNativeDeviceV1* device = nullptr;
        CHECK(provider->QueryInterface(IID_IA3dNativeDeviceV1, reinterpret_cast<void**>(&device)) == S_OK);
        DWORD supported = 0;
        CHECK(device->InitializeDevice(nullptr, 0, 1, &supported) == S_OK);
        auto* manager = dynamic_cast<ResMan*>(provider);
        manager->StopNativeWorker();
        CHECK(manager->SetResourceManagerMode(A3D_RESOURCE_MODE_DYNAMIC) == S_OK);
        WAVEFORMATEX format{WAVE_FORMAT_PCM, 1, 22050, 44100, 2, 16, 0};
        DSBUFFERDESC desc{};
        desc.dwSize = sizeof(desc);
        desc.dwBufferBytes = kBytes;
        desc.lpwfxFormat = &format;
        IDirectSoundBuffer* buffer = nullptr;
        CHECK(manager->CreateSoundBuffer(&desc, &buffer, nullptr) == S_OK);
        CHECK(buffer->Play(0, 0, DSBPLAY_LOOPING) == S_OK);
        A3dNativeFailAcquisitionAfter(failure);
        CHECK(manager->TickNative(200) == E_OUTOFMEMORY);
        CHECK(manager->GetSoftwareStreamCount() == 0);
        A3dNativeFailAcquisitionAfter(-1);
        CHECK(manager->TickNative(400) == S_OK && manager->GetSoftwareStreamCount() == 1);
        buffer->Release();
        device->Release();
        provider->Release();
    }
}

void HeldShutdown() {
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
    desc.dwBufferBytes = kBytes;
    desc.lpwfxFormat = &format;
    IDirectSoundBuffer* buffer = nullptr;
    CHECK(manager->CreateSoundBuffer(&desc, &buffer, nullptr) == S_OK);
    IA3dNativeNotifyV1* notify = nullptr;
    CHECK(buffer->QueryInterface(IID_IA3dNativeNotifyV1, reinterpret_cast<void**>(&notify)) == S_OK);
    auto* event = Event();
    A3dNativeNotificationPositionV1 position{A3D_NATIVE_NOTIFICATION_STOP, 0, event};
    CHECK(notify->SetNotificationPositions(1, &position) == S_OK);
    CHECK(buffer->Play(0, 0, DSBPLAY_LOOPING) == S_OK);
    manager->ShutdownNative();
    CHECK(Poll(event));
    CHECK(buffer->Play(0, 0, 0) == E_ABORT);
    CHECK(notify->SetNotificationPositions(1, &position) == E_ABORT);
    CHECK(event->Signal() == S_OK);
    notify->Release();
    buffer->Release();
    event->Close();
    event->Release();
    CHECK(device->InitializeDevice(nullptr, 0, 1, &supported) == S_OK);
    device->Release();
    provider->Release();
}

void PriorityPressure(bool loopers = false) {
    IUnknown* provider = nullptr;
    CHECK(A3dCreateNativeProvider(&provider) == S_OK);
    IA3dNativeDeviceV1* device = nullptr;
    CHECK(provider->QueryInterface(IID_IA3dNativeDeviceV1, reinterpret_cast<void**>(&device)) == S_OK);
    DWORD supported = 0;
    CHECK(device->InitializeDevice(nullptr, 0, 1, &supported) == S_OK);
    auto* manager = dynamic_cast<ResMan*>(provider);
    CHECK(manager);
    manager->StopNativeWorker();
    CHECK(manager->SetResourceManagerMode(A3D_RESOURCE_MODE_DYNAMIC) == S_OK);
    WAVEFORMATEX format{WAVE_FORMAT_PCM, 1, 44100, 88200, 2, 16, 0};
    DSBUFFERDESC desc{};
    desc.dwSize = sizeof(desc);
    desc.dwBufferBytes = kBytes;
    desc.lpwfxFormat = &format;
    std::array<IDirectSoundBuffer*, 10> buffers{};
    for (auto& buffer : buffers) {
        CHECK(manager->CreateSoundBuffer(&desc, &buffer, nullptr) == S_OK);
        CHECK(buffer->Play(0, 0, DSBPLAY_LOOPING) == S_OK);
    }
    auto* highest = dynamic_cast<ResManBuffer*>(buffers.back());
    CHECK(highest->SetPriority(loopers ? 0 : 100) == S_OK);
    if (loopers) {
        CHECK(manager->SetResourceManagerMode(A3D_RESOURCE_MODE_DYNAMIC_LOOPERS) == S_OK);
        for (std::size_t i = 0; i + 1 < buffers.size(); ++i) {
            CHECK(buffers[i]->Play(0, 0, 0) == S_OK);
            CHECK(dynamic_cast<ResManBuffer*>(buffers[i])->SetPriority(1000) == S_OK);
        }
    }
    CHECK(manager->TickNative(200) == S_OK);
    DalBufferInfo* assigned = nullptr;
    CHECK(highest->GetRenderStream(&assigned) == S_OK && assigned);
    auto* next = dynamic_cast<ResManBuffer*>(buffers[8]);
    CHECK(next->SetPriority(200) == S_OK);
    CHECK(manager->TickNative(400) == S_OK);
    CHECK(next->GetRenderStream(&assigned) == S_OK);
    if (!loopers) {
        CHECK(assigned);
    }
    CHECK(highest->GetRenderStream(&assigned) == S_OK && assigned);
    CHECK(manager->GetSoftwareStreamCount() == 8);
    for (auto* buffer : buffers) {
        buffer->Release();
    }
    device->Release();
    provider->Release();
}

void ResourceModes() {
    const char* driver = std::getenv("SDL_AUDIO_DRIVER");
    CHECK(driver && !std::strcmp(driver, "dummy"));
    IUnknown* provider = nullptr;
    CHECK(A3dCreateNativeProvider(&provider) == S_OK);
    IA3dNativeDeviceV1* device = nullptr;
    CHECK(provider->QueryInterface(IID_IA3dNativeDeviceV1, reinterpret_cast<void**>(&device)) == S_OK);
    DWORD supported = 0;
    CHECK(device->InitializeDevice(nullptr, 0, 1, &supported) == S_OK);
    IA3d2* resource = nullptr;
    IDirectSound* sound = nullptr;
    CHECK(provider->QueryInterface(IID_IA3d2, reinterpret_cast<void**>(&resource)) == S_OK);
    CHECK(provider->QueryInterface(IID_IDirectSound, reinterpret_cast<void**>(&sound)) == S_OK);
    WAVEFORMATEX format{WAVE_FORMAT_PCM, 1, 44100, 88200, 2, 16, 0};
    DSBUFFERDESC desc{};
    desc.dwSize = sizeof(desc);
    desc.dwBufferBytes = kBytes;
    desc.lpwfxFormat = &format;
    for (DWORD mode = A3D_RESOURCE_MODE_OFF; mode <= A3D_RESOURCE_MODE_LAST; ++mode) {
        CHECK(resource->SetResourceManagerMode(mode) == S_OK);
        IDirectSoundBuffer* rejected = nullptr;
        format.cbSize = 1;
        CHECK(sound->CreateSoundBuffer(&desc, &rejected, nullptr) == E_INVALIDARG && !rejected);
        format.cbSize = 0;
        --format.nAvgBytesPerSec;
        CHECK(sound->CreateSoundBuffer(&desc, &rejected, nullptr) == E_INVALIDARG && !rejected);
        ++format.nAvgBytesPerSec;
        IDirectSoundBuffer* buffer = nullptr;
        CHECK(sound->CreateSoundBuffer(&desc, &buffer, nullptr) == S_OK);
        IA3dNativeNotifyV1* notify = nullptr;
        CHECK(buffer->QueryInterface(IID_IA3dNativeNotifyV1, reinterpret_cast<void**>(&notify)) == S_OK);
        A3dNativeNotificationCapsV1 caps{sizeof(caps), 1, 0, 0, 0, {}};
        CHECK(notify->GetNotificationCaps(&caps) == S_OK);
        CHECK(caps.cursorKind == (mode == A3D_RESOURCE_MODE_OFF ? A3D_NATIVE_CURSOR_RENDER_CONSUMED :
            A3D_NATIVE_CURSOR_STREAM_SUBMITTED));
        auto* event = Event();
        A3dNativeNotificationPositionV1 position{0, 0, event};
        CHECK(notify->SetNotificationPositions(1, &position) == S_OK);
        CHECK(buffer->Play(0, 0, DSBPLAY_LOOPING) == S_OK);
        A3dNativeWaitStatusV1 status{};
        CHECK(event->Wait(kWaitMilliseconds, nullptr, &status) == S_OK);
        CHECK(status == A3dNativeWaitStatusV1::Signaled);
        CHECK(resource->SetResourceManagerMode((mode + 1) % (A3D_RESOURCE_MODE_LAST + 1)) == S_OK);
        const DWORD originalKind = caps.cursorKind;
        CHECK(notify->GetNotificationCaps(&caps) == S_OK && caps.cursorKind == originalKind);
        CHECK(buffer->Stop() == S_OK);
        CHECK(notify->SetNotificationPositions(0, nullptr) == S_OK);
        notify->Release();
        buffer->Release();
        event->Close();
        event->Release();
    }
    sound->Release();
    resource->Release();
    device->Release();
    provider->Release();
}

void NaturalStreamTail() {
    A3dNativeMixer mixer;
    ResMan owner;
    ResManStreamBuffer source(static_cast<IDirectSound*>(&owner), A3D_RESOURCE_MODE_DYNAMIC);
    WAVEFORMATEX format{WAVE_FORMAT_PCM, 1, 22050, 44100, 2, 16, 0};
    a3d_platform_buffer_desc_t desc{sizeof(desc), 0, kBytes, 0, &format};
    CHECK(source.Init(&desc, std::malloc(sizeof(DWORD))) == S_OK);
    DSBUFFERDESC outputDesc{};
    outputDesc.dwSize = sizeof(outputDesc);
    outputDesc.dwBufferBytes = 64;
    outputDesc.lpwfxFormat = &format;
    IDirectSoundBuffer* output = nullptr;
    CHECK(mixer.CreateBuffer(&outputDesc, &output) == S_OK);
    a3d_render_description_t streamDesc{A3D_RENDER_WRITE_BUFFER, 64, 22050, 16, 64};
    DalBufferInfo stream(nullptr, output, &streamDesc, 2);
    CHECK(source.SetStream(&stream) == S_OK);
    auto* stopped = Event();
    A3dNativeNotificationPositionV1 position{A3D_NATIVE_NOTIFICATION_STOP, 0, stopped};
    CHECK(source.SetNotificationPositions(1, &position) == S_OK);
    CHECK(source.Play(0, 0, 0) == S_OK);
    CHECK(source.Update(0) == S_OK);
    DWORD status = 0;
    CHECK(output->GetStatus(&status) == S_OK && (status & DSBSTATUS_PLAYING));
    CHECK(!Poll(stopped));
    std::array<SHORT, 64> pcm{};
    CHECK(mixer.Render(pcm.data(), 32) == S_OK);
    CHECK(source.Update(10) == S_OK);
    CHECK(Poll(stopped));
    CHECK(source.Stop() == S_OK && !Poll(stopped));
    CHECK(source.SetStream(nullptr) == S_OK);
    output->Release();
    stopped->Close();
    stopped->Release();
}

void StreamOwnership() {
    ResMan owner;
    ResManStreamBuffer original(static_cast<IDirectSound*>(&owner), A3D_RESOURCE_MODE_DYNAMIC);
    WAVEFORMATEX format{WAVE_FORMAT_PCM, 1, 44100, 88200, 2, 16, 0};
    a3d_platform_buffer_desc_t desc{sizeof(desc), 0, kBytes, 0, &format};
    CHECK(original.Init(&desc, std::malloc(sizeof(DWORD))) == S_OK);
    DSBCAPS caps{};
    caps.dwSize = sizeof(caps);
    CHECK(original.GetCaps(&caps) == S_OK);
    CHECK(caps.dwBufferBytes == kBytes && (caps.dwFlags & DSBCAPS_LOCSOFTWARE) &&
        !(caps.dwFlags & (DSBCAPS_LOCHARDWARE | DSBCAPS_CTRLPOSITIONNOTIFY)));
    format.nSamplesPerSec = 12345;
    WAVEFORMATEX copied{};
    DWORD written = 0;
    CHECK(original.GetFormat(&copied, sizeof(copied), &written) == S_OK);
    CHECK(written == sizeof(copied) && copied.nSamplesPerSec == 44100);
    IDirectSound3DBuffer* spatial = nullptr;
    CHECK(original.QueryInterface(IID_IDirectSound3DBuffer, reinterpret_cast<void**>(&spatial)) == S_OK);
    CHECK(spatial->SetPosition(1, 2, 3, DS3D_IMMEDIATE) == S_OK);
    D3DVECTOR position{};
    CHECK(spatial->GetPosition(&position) == S_OK && position.x == 1 && position.y == 2 && position.z == 3);
    IUnknown* identity = nullptr;
    CHECK(spatial->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&identity)) == S_OK);
    CHECK(identity == static_cast<IDirectSoundBuffer*>(&original));
    identity->Release();
    spatial->Release();
    A3DCTRL_SRC_SUPER control{};
    control.flPitch = 1;
    CHECK(original.SetA3dSuperCtrl(&control, 0) == S_OK);
    control.flPitch = std::numeric_limits<float>::quiet_NaN();
    ResManStreamBuffer duplicate(static_cast<IDirectSound*>(&owner), A3D_RESOURCE_MODE_DYNAMIC);
    CHECK(duplicate.Duplicate(&original) == S_OK);
    CHECK(duplicate.GetFormat(&copied, sizeof(copied), &written) == S_OK && copied.nSamplesPerSec == 44100);
    void* data = nullptr;
    DWORD bytes = 0;
    CHECK(original.Lock(0, kBytes, &data, &bytes, nullptr, nullptr, 0) == S_OK);
    std::memset(data, 0x37, bytes);
    CHECK(original.Unlock(data, bytes, nullptr, 0) == S_OK);
    CHECK(duplicate.Lock(0, kBytes, &data, &bytes, nullptr, nullptr, 0) == S_OK);
    CHECK(static_cast<BYTE*>(data)[kBytes - 1] == 0x37);
    CHECK(duplicate.Unlock(data, bytes, nullptr, 0) == S_OK);

    A3dNativeMixer mixer;
    IDirectSoundBuffer* output = nullptr;
    DSBUFFERDESC outputDesc{};
    outputDesc.dwSize = sizeof(outputDesc);
    outputDesc.dwBufferBytes = kBytes;
    outputDesc.lpwfxFormat = &copied;
    CHECK(mixer.CreateBuffer(&outputDesc, &output) == S_OK);
    a3d_render_description_t streamDesc{A3D_RENDER_WRITE_BUFFER, kBytes, 44100, 16, kBytes};
    constexpr DWORD kPooledStream = 2; // Retained DalBufferInfo pooled-stream kind.
    DalBufferInfo stream(nullptr, output, &streamDesc, kPooledStream);
    CHECK(duplicate.SetStream(&stream) == S_OK); // Both stored copies survive caller mutation.
    CHECK(duplicate.SetA3dSuperCtrl(&control, 0) == E_INVALIDARG); // Actual leaf control rejection propagates.
    CHECK(duplicate.Play(0, 0, DSBPLAY_LOOPING) == S_OK);
    CHECK(output->Lock(0, kBytes, &data, &bytes, nullptr, nullptr, 0) == S_OK);
    CHECK(FAILED(duplicate.Update(0))); // Actual destination writer conflict propagates.
    DWORD cursor = kBytes;
    CHECK(duplicate.GetCurrentPosition(&cursor, nullptr) == S_OK && cursor == 0);
    CHECK(output->Unlock(data, bytes, nullptr, 0) == S_OK);
    CHECK(duplicate.Update(0) == S_OK);
    CHECK(duplicate.Stop() == S_OK);
    CHECK(duplicate.SetStream(nullptr) == S_OK);
    output->Release();
}

}

int main(int argc, char** argv) {
    CHECK(argc == 2);
    if (!std::strcmp(argv[1], "wrapped_stream_tail")) {
        WrappedStreamTail();
    } else if (!std::strcmp(argv[1], "factory_allocation_failure")) {
        FactoryAllocationFailure();
    } else if (!std::strcmp(argv[1], "concurrent_resource_modes")) {
        ConcurrentModes();
    } else if (!std::strcmp(argv[1], "pool_failure_forwarding")) {
        PoolFailures();
    } else if (!std::strcmp(argv[1], "loopers_pending_streams")) {
        PriorityPressure(true);
    } else if (!std::strcmp(argv[1], "software_capacity_not_hardware")) {
        Capacity();
    } else if (!std::strcmp(argv[1], "guid_copy_survives")) {
        GuidCopy();
    } else if (!std::strcmp(argv[1], "no_factory_recursion")) {
        NoFactoryRecursion();
    } else if (!std::strcmp(argv[1], "tick_wrap_expiry")) {
        StreamSplitProgress();
    } else if (!std::strcmp(argv[1], "legacy_notify_views")) {
        LegacyViews();
    } else if (!std::strcmp(argv[1], "final_release_stops_workers")) {
        HeldShutdown();
    } else if (!std::strcmp(argv[1], "init_fail_reinit")) {
        InitFailReinit();
    } else if (!std::strcmp(argv[1], "priority_pressure")) {
        PriorityPressure();
    } else if (!std::strcmp(argv[1], "resource_modes")) {
        ResourceModes();
    } else if (!std::strcmp(argv[1], "typed_init_no_dword_pointer")) {
        TypedProvider();
    } else if (!std::strcmp(argv[1], "resource_stream_ownership")) {
        StreamOwnership();
        NaturalStreamTail();
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
