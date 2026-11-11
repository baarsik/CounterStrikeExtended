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
#include <condition_variable>
#include <mutex>
#include <chrono>
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

struct Fixture {
    A3dNativeMixer mixer;
    IDirectSoundBuffer* buffer = nullptr;
    IA3dNativeNotifyV1* notify = nullptr;
    Fixture() {
        WAVEFORMATEX format{WAVE_FORMAT_PCM, 1, 22050, 44100, 2, 16, 0};
        DSBUFFERDESC desc{};
        desc.dwSize = sizeof(desc);
        desc.dwBufferBytes = kBytes;
        desc.lpwfxFormat = &format;
        CHECK(mixer.CreateBuffer(&desc, &buffer) == S_OK);
        CHECK(buffer->QueryInterface(IID_IA3dNativeNotifyV1, reinterpret_cast<void**>(&notify)) == S_OK);
        A3dNativeNotificationCapsV1 caps{};
        caps.structureBytes = sizeof(caps);
        caps.version = 1;
        CHECK(notify->GetNotificationCaps(&caps) == S_OK);
        CHECK(caps.flags == 0x1f && caps.maximumPositions == 1024);
        CHECK(caps.cursorKind == A3D_NATIVE_CURSOR_RENDER_CONSUMED);
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
        CHECK(mixer.Render(output.data(), bytes / 2) == S_OK);
    }
};

void StreamCrossings() {
    A3dNativeMixer mixer;
    ResMan owner;
    ResManStreamBuffer buffer(static_cast<IDirectSound*>(&owner), A3D_RESOURCE_MODE_DYNAMIC);
    WAVEFORMATEX format{WAVE_FORMAT_PCM, 1, 44100, 88200, 2, 16, 0};
    a3d_platform_buffer_desc_t desc{sizeof(desc), 0, kBytes, 0, &format};
    CHECK(buffer.Init(&desc, std::malloc(sizeof(DWORD))) == S_OK);
    IA3dNativeNotifyV1* notify = nullptr;
    CHECK(buffer.QueryInterface(IID_IA3dNativeNotifyV1, reinterpret_cast<void**>(&notify)) == S_OK);
    A3dNativeNotificationCapsV1 caps{};
    caps.structureBytes = sizeof(caps);
    caps.version = 1;
    CHECK(notify->GetNotificationCaps(&caps) == S_OK);
    CHECK(caps.cursorKind == A3D_NATIVE_CURSOR_STREAM_SUBMITTED && caps.flags == 0x1f);
    const DWORD offsets[]{12, 15, 0, 3, 4, 11};
    std::array<IA3dNativeEventV1*, 6> events{};
    std::array<A3dNativeNotificationPositionV1, 6> positions{};
    for (std::size_t i = 0; i < events.size(); ++i) {
        events[i] = Event();
        positions[i] = {offsets[i], 0, events[i]};
    }
    CHECK(notify->SetNotificationPositions(positions.size(), positions.data()) == S_OK);
    struct Case {
        DWORD old;
        DWORD advance;
        unsigned mask;
    };
    const Case cases[]{{12, 8, 0x0f}, {12, 4, 0x03}, {12, 32, 0x3f}, {0, 4, 0x0c}};
    for (const auto& test : cases) {
        DSBUFFERDESC outputDesc{};
        outputDesc.dwSize = sizeof(outputDesc);
        outputDesc.dwBufferBytes = test.advance;
        outputDesc.lpwfxFormat = &format;
        IDirectSoundBuffer* output = nullptr;
        CHECK(mixer.CreateBuffer(&outputDesc, &output) == S_OK);
        a3d_render_description_t streamDesc{A3D_RENDER_WRITE_BUFFER, test.advance, 44100, 16, test.advance};
        DalBufferInfo stream(nullptr, output, &streamDesc, 2);
        CHECK(buffer.SetStream(&stream) == S_OK);
        CHECK(buffer.SetCurrentPosition(test.old) == S_OK);
        CHECK(buffer.Play(0, 0, DSBPLAY_LOOPING) == S_OK);
        CHECK(buffer.Update(0) == S_OK);
        for (std::size_t i = 0; i < events.size(); ++i) {
            CHECK(Poll(events[i]) == ((test.mask & (1u << i)) != 0));
        }
        CHECK(buffer.Stop() == S_OK);
        CHECK(buffer.SetStream(nullptr) == S_OK);
        output->Release();
    }
    CHECK(notify->SetNotificationPositions(0, nullptr) == S_OK);
    notify->Release();
    for (auto* event : events) {
        event->Close();
        event->Release();
    }
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
    CHECK(device->InitializeDevice(nullptr, 0, 1, &supported) == S_OK && supported == 0);
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
        CHECK(barrier.condition.wait_for(lock, std::chrono::seconds(2), [&] {
            return barrier.released;
        }));
    }
    void Wait() {
        std::unique_lock<std::mutex> lock(mutex);
        CHECK(condition.wait_for(lock, std::chrono::seconds(2), [&] {
            return reached;
        }));
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
}

int main(int argc, char** argv) {
    CHECK(argc == 2);
    if (!std::strcmp(argv[1], "pool_failure_forwarding")) {
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
    } else if (!std::strcmp(argv[1], "notification_crossings")) {
        Crossings();
        StreamCrossings();
        StreamOwnership();
        NaturalStreamTail();
        Shutdown();
    } else if (!std::strcmp(argv[1], "notification_replace_remove")) {
        Replace();
        Fence(false);
        Fence(true);
    } else if (!std::strcmp(argv[1], "notification_waiter_reentry")) {
        Reentry();
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
