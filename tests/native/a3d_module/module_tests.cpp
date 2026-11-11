#include "caller_interfaces.h"
#include "cse_a3d_abi.h"
#include <atomic>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <dlfcn.h>
#include <thread>
#include <vector>

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); std::exit(1); } } while (false)

namespace {
constexpr auto expected = cse::a3d_abi::Describe<IA3dSource202, IA3dGeomTarget, IA3dListenerTarget>();
constexpr IID unknownIid{0x12345678, 0x1234, 0x5678, {1, 2, 3, 4, 5, 6, 7, 8}};
constexpr DWORD kDirectFeature = 0x08;
constexpr DWORD kBufferBytes = 4096;
constexpr DWORD kSampleRate = 22050;
using GetFactory = HRESULT (*)(REFCLSID, REFIID, void**);
using CanUnload = HRESULT (*)();
using CreateEvent = HRESULT (*)(const A3dNativeEventDescV1*, IA3dNativeEventV1**);

// Test observation of existing production counters, never replacement ownership.
struct EventCounts {
    std::size_t objects;
    std::size_t retainedStates;
    std::size_t waits;
    std::size_t calls;
};
using EventSnapshot = EventCounts (*)();
using OwnerSnapshot = std::array<std::size_t, 14> (*)();

struct Module {
    void* handle = nullptr;
    cse::abi::GetModuleAbi describe = nullptr;
    GetFactory factory = nullptr;
    CanUnload unload = nullptr;
    CreateEvent event = nullptr;
    unsigned factoryCalls = 0;
    EventSnapshot events = nullptr;
    OwnerSnapshot owners = nullptr;

    explicit Module(const char* path) {
        handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (!handle) {
            std::fprintf(stderr, "%s\n", dlerror());
        }
        CHECK(handle);
        describe = reinterpret_cast<cse::abi::GetModuleAbi>(dlsym(handle, "CseGetModuleAbi"));
        CHECK(describe != nullptr);
        CHECK(cse::abi::Check(expected, describe()) == cse::abi::Compatibility::Compatible);
        factory = reinterpret_cast<GetFactory>(dlsym(handle, "DllGetClassObject"));
        unload = reinterpret_cast<CanUnload>(dlsym(handle, "DllCanUnloadNow"));
        event = reinterpret_cast<CreateEvent>(dlsym(handle, "CseCreateA3dNativeEventV1"));
        CHECK(factory && unload && event);
        events = reinterpret_cast<EventSnapshot>(dlsym(handle, "_Z22A3dNativeEventLifetimev"));
        owners = reinterpret_cast<OwnerSnapshot>(dlsym(handle, "_Z15A3dNativeOwnersv"));
        CHECK(events && owners);
        CHECK(unload() == S_OK);
    }
    ~Module() {
        // The host library lease covers every virtual call and joined waiter.
        for (auto count : owners()) {
            CHECK(count == 0);
        }
        const auto snapshot = events();
        CHECK(!snapshot.objects && !snapshot.retainedStates && !snapshot.waits && !snapshot.calls);
        CHECK(unload() == S_OK);
        CHECK(dlclose(handle) == 0);
    }
    bool Admit(const cse::abi::Descriptor* offered) {
        if (cse::abi::Check(expected, offered) != cse::abi::Compatibility::Compatible) {
            return false;
        }
        ++factoryCalls;
        IClassFactory* result = nullptr;
        CHECK(factory(CLSID_A3dApi, IID_IClassFactory, reinterpret_cast<void**>(&result)) == S_OK);
        result->Release();
        return true;
    }
    IClassFactory* Factory() {
        IClassFactory* result = nullptr;
        CHECK(factory(CLSID_A3dApi, IID_IClassFactory, reinterpret_cast<void**>(&result)) == S_OK);
        return result;
    }
    template<class T> T* Create(REFIID iid) {
        auto* classFactory = Factory();
        T* result = nullptr;
        CHECK(classFactory->CreateInstance(nullptr, iid, reinterpret_cast<void**>(&result)) == S_OK);
        classFactory->Release();
        CHECK(result);
        return result;
    }
    IA3dNativeEventV1* Event(bool manual = false) {
        A3dNativeEventDescV1 desc{sizeof(desc), 1, manual ? A3D_NATIVE_EVENT_MANUAL_RESET : A3D_NATIVE_EVENT_AUTO_RESET, 0};
        IA3dNativeEventV1* result = nullptr;
        CHECK(event(&desc, &result) == S_OK);
        return result;
    }
};

template<class T> T* Query(IUnknown* object, REFIID iid) {
    T* result = nullptr;
    CHECK(object->QueryInterface(iid, reinterpret_cast<void**>(&result)) == S_OK);
    CHECK(result);
    return result;
}

void Canonical(IUnknown* first, IUnknown* second) {
    auto* a = Query<IUnknown>(first, IID_IUnknown);
    auto* b = Query<IUnknown>(second, IID_IUnknown);
    CHECK(a == b);
    a->Release();
    b->Release();
}

WAVEFORMATEX Format() {
    return {WAVE_FORMAT_PCM, 1, kSampleRate, kSampleRate * 2, 2, 16, 0};
}

IDirectSound* Provider(Module& module) {
    auto* sound = module.Create<IDirectSound>(IID_IDirectSound);
    auto* device = Query<IA3dNativeDeviceV1>(sound, IID_IA3dNativeDeviceV1);
    DWORD features = UINT32_MAX;
    CHECK(device->InitializeDevice(nullptr, kDirectFeature, 1, &features) == S_OK);
    CHECK(features == kDirectFeature);
    device->Release();
    return sound;
}

IDirectSoundBuffer* Buffer(IDirectSound* sound, bool stream = true) {
    auto* api = Query<IA3d2>(sound, IID_IA3d2);
    CHECK(api->SetResourceManagerMode(stream ? A3D_RESOURCE_MODE_DYNAMIC : A3D_RESOURCE_MODE_OFF) == S_OK);
    api->Release();
    auto format = Format();
    DSBUFFERDESC desc{};
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DSBCAPS_CTRL3D | DSBCAPS_CTRLFREQUENCY | DSBCAPS_CTRLVOLUME;
    desc.dwBufferBytes = kBufferBytes;
    desc.lpwfxFormat = &format;
    IDirectSoundBuffer* buffer = nullptr;
    CHECK(sound->CreateSoundBuffer(&desc, &buffer, nullptr) == S_OK);
    return buffer;
}

void WritePcm(IDirectSoundBuffer* buffer) {
    void* first = nullptr;
    void* second = nullptr;
    DWORD firstBytes = 0;
    DWORD secondBytes = 0;
    CHECK(buffer->Lock(0, kBufferBytes, &first, &firstBytes, &second, &secondBytes, 0) == S_OK);
    CHECK(first && firstBytes == kBufferBytes && !second && !secondBytes);
    auto* samples = static_cast<std::int16_t*>(first);
    for (DWORD i = 0; i < kBufferBytes / sizeof(*samples); ++i) {
        samples[i] = (i % 32 < 16) ? 12000 : -12000;
    }
    CHECK(buffer->Unlock(first, firstBytes, second, secondBytes) == S_OK);
}

void RejectAbi(Module& module) {
    CHECK(!module.Admit(nullptr));
    const auto original = *module.describe();
    for (std::size_t field = 0; field < 8; ++field) {
        auto mutated = original;
        reinterpret_cast<std::uint32_t*>(&mutated)[field] ^= 0x10;
        CHECK(!module.Admit(&mutated));
    }
    for (std::size_t index = 0; index < original.tableCount; ++index) {
        auto mutated = original;
        mutated.tables[index].signature ^= 1;
        CHECK(!module.Admit(&mutated));
        mutated = original;
        mutated.tables[index].identifier ^= 1;
        CHECK(!module.Admit(&mutated));
        mutated = original;
        mutated.tables[index].reserved = 1;
        CHECK(!module.Admit(&mutated));
    }
    CHECK(module.factoryCalls == 0);
    CHECK(module.Admit(&original));
    CHECK(module.factoryCalls == 1);
}

void ClassIidAggregation(Module& module) {
    void* output = reinterpret_cast<void*>(1);
    CHECK(module.factory(unknownIid, IID_IClassFactory, &output) == CLASS_E_CLASSNOTAVAILABLE);
    CHECK(output == nullptr);
    CHECK(module.factory(CLSID_A3dApi, unknownIid, &output) == E_NOINTERFACE);
    CHECK(output == nullptr);
    CHECK(module.factory(CLSID_A3dApi, IID_IClassFactory, nullptr) == E_POINTER);
    auto* factory = module.Factory();
    CHECK(factory->CreateInstance(factory, IID_IA3d3, &output) == CLASS_E_NOAGGREGATION);
    CHECK(output == nullptr);
    CHECK(factory->CreateInstance(nullptr, IID_IA3d3, nullptr) == E_POINTER);
    CHECK(factory->QueryInterface(IID_IUnknown, nullptr) == E_POINTER);
    factory->Release();
    for (const auto& clsid : {CLSID_A3dApi, CLSID_A3d}) {
        CHECK(module.factory(clsid, IID_IClassFactory, reinterpret_cast<void**>(&factory)) == S_OK);
        IUnknown* instance = nullptr;
        CHECK(factory->CreateInstance(nullptr, IID_IUnknown, reinterpret_cast<void**>(&instance)) == S_OK);
        instance->Release();
        factory->Release();
    }
}

void CanonicalUnknown(Module& module) {
    auto* api = module.Create<IA3d3>(IID_IA3d3);
    auto* geometry = Query<IA3dGeomTarget>(api, IID_IA3dGeom);
    auto* listener = Query<IA3dListenerTarget>(api, IID_IA3dListener);
    Canonical(api, geometry);
    Canonical(api, listener);
    constexpr IID privateScene{0x103e7222, 0x0113, 0x11d2, {0x90, 0xfb, 0x00, 0x60, 0x08, 0xa1, 0xf4, 0x41}};
    void* unqualified = reinterpret_cast<void*>(1);
    CHECK(api->QueryInterface(privateScene, &unqualified) == E_NOINTERFACE && !unqualified);
    auto* api2 = Query<IA3d2>(api, IID_IA3d2);
    Canonical(api, api2);
    api2->Release();
    geometry->Release();
    listener->Release();
    api->Release();
    auto* sound = Provider(module);
    auto* device = Query<IA3dNativeDeviceV1>(sound, IID_IA3dNativeDeviceV1);
    Canonical(sound, device);
    auto* buffer = Buffer(sound);
    auto* notify = Query<IA3dNativeNotifyV1>(buffer, IID_IA3dNativeNotifyV1);
    auto* spatial = Query<IDirectSound3DBuffer>(buffer, IID_IDirectSound3DBuffer);
    Canonical(buffer, notify);
    Canonical(buffer, spatial);
    spatial->Release();
    notify->Release();
    buffer->Release();
    device->Release();
    sound->Release();
}

void QiFailureCleanup(Module& module) {
    auto* factory = module.Factory();
    for (unsigned i = 0; i < 16; ++i) {
        void* output = reinterpret_cast<void*>(1);
        CHECK(FAILED(factory->CreateInstance(nullptr, unknownIid, &output)));
        CHECK(output == nullptr);
    }
    factory->Release();
    CHECK(module.unload() == S_OK);
}

void FactoryLocks(Module& module) {
    auto* factory = module.Factory();
    CHECK(module.unload() == S_FALSE);
    CHECK(FAILED(factory->LockServer(FALSE)));
    CHECK(factory->LockServer(TRUE) == S_OK);
    CHECK(factory->LockServer(TRUE) == S_OK);
    factory->Release();
    CHECK(module.unload() == S_FALSE);
    factory = module.Factory();
    CHECK(factory->LockServer(FALSE) == S_OK);
    CHECK(module.unload() == S_FALSE);
    CHECK(factory->LockServer(FALSE) == S_OK);
    factory->Release();
    CHECK(module.unload() == S_OK);
}

void SourcePlayback(Module& module) {
    auto* api = module.Create<IA3d3>(IID_IA3d3);
    CHECK(api->Init(nullptr, kDirectFeature, 0) == S_OK);
    IA3dSource* raw = nullptr;
    CHECK(api->NewSource(0, &raw) == S_OK);
    auto* source = reinterpret_cast<IA3dSource202*>(raw);
    auto* sourceView = Query<IA3dSource>(raw, IID_IA3dSource);
    Canonical(source, sourceView);
    sourceView->Release();
    auto format = Format();
    CHECK(source->SetWaveFormat(&format) == S_OK);
    CHECK(source->AllocateWaveData(kBufferBytes) == S_OK);
    void* first = nullptr;
    void* second = nullptr;
    DWORD firstBytes = 0;
    DWORD secondBytes = 0;
    CHECK(source->Lock(0, kBufferBytes, &first, &firstBytes, &second, &secondBytes, 0) == S_OK);
    CHECK(firstBytes == kBufferBytes);
    std::memset(first, 0x25, firstBytes);
    CHECK(source->Unlock(first, firstBytes, second, secondBytes) == S_OK);
    WAVEFORMATEX readFormat{};
    CHECK(source->GetWaveFormat(&readFormat) == S_OK && readFormat.nSamplesPerSec == kSampleRate);
    CHECK(source->GetWaveSize() == static_cast<HRESULT>(kBufferBytes));
    DWORD sourceType = UINT32_MAX;
    CHECK(source->GetType(&sourceType) == S_OK && sourceType == 0);
    float vector[6]{1, 2, 3, 0, 1, 0};
    float readVector[6]{};
    CHECK(source->SetPosition3fv(vector) == S_OK);
    CHECK(source->GetPosition3fv(readVector) == S_OK);
    CHECK(readVector[0] == 1 && readVector[1] == 2 && readVector[2] == 3);
    CHECK(source->SetPosition3f(3, 2, 1) == S_OK);
    CHECK(source->GetPosition3f(&readVector[0], &readVector[1], &readVector[2]) == S_OK);
    CHECK(readVector[0] == 3 && readVector[2] == 1);
    CHECK(source->SetVelocity3fv(vector) == S_OK);
    CHECK(source->GetVelocity3fv(readVector) == S_OK && readVector[0] == 1);
    CHECK(source->SetVelocity3f(0, 0, 0) == S_OK);
    CHECK(source->GetVelocity3f(&readVector[0], &readVector[1], &readVector[2]) == S_OK);
    CHECK(readVector[0] == 0 && readVector[1] == 0 && readVector[2] == 0);
    float zeroAngles[3]{};
    CHECK(source->SetOrientationAngles3fv(zeroAngles) == S_OK);
    CHECK(source->GetOrientationAngles3fv(readVector) == S_OK);
    CHECK(source->SetOrientationAngles3f(0, 0, 0) == S_OK);
    CHECK(source->GetOrientationAngles3f(&readVector[0], &readVector[1], &readVector[2]) == S_OK);
    CHECK(std::isfinite(readVector[0]));
    float orientation[6]{0, 0, 1, 0, 1, 0};
    CHECK(source->SetOrientation6fv(orientation) == S_OK);
    CHECK(source->GetOrientation6fv(readVector) == S_OK);
    CHECK(source->SetOrientation6f(0, 0, 1, 0, 1, 0) == S_OK);
    CHECK(source->GetOrientation6f(&readVector[0], &readVector[1], &readVector[2],
        &readVector[3], &readVector[4], &readVector[5]) == S_OK);
    CHECK(readVector[2] == 1 && readVector[4] == 1);
    CHECK(source->SetCone(60, 120, 0.5f) == S_OK);
    CHECK(source->GetCone(&readVector[0], &readVector[1], &readVector[2]) == S_OK);
    CHECK(readVector[0] == 60 && readVector[1] == 120 && readVector[2] == 0.5f);
    DWORD mute = 0;
    CHECK(source->SetMinMaxDistance(1, 100, 1) == S_OK);
    CHECK(source->GetMinMaxDistance(&readVector[0], &readVector[1], &mute) == S_OK);
    CHECK(readVector[0] == 1 && readVector[1] == 100 && mute == 1);
    using SetScalar = HRESULT (IA3dSource202::*)(float);
    using GetScalar = HRESULT (IA3dSource202::*)(float*);
    const SetScalar setters[]{&IA3dSource202::SetPitch, &IA3dSource202::SetDopplerScale,
        &IA3dSource202::SetDistanceModelScale, &IA3dSource202::SetEq};
    const GetScalar getters[]{&IA3dSource202::GetPitch, &IA3dSource202::GetDopplerScale,
        &IA3dSource202::GetDistanceModelScale, &IA3dSource202::GetEq};
    for (unsigned i = 0; i < 4; ++i) {
        CHECK((source->*setters[i])(0.75f) == S_OK);
        CHECK((source->*getters[i])(&readVector[0]) == S_OK && readVector[0] == 0.75f);
    }
    CHECK(FAILED(source->SetPriority(0.5f)));
    CHECK(FAILED(source->GetPriority(&readVector[0])));
    CHECK(source->SetRenderMode(0) == S_OK);
    CHECK(source->GetRenderMode(&sourceType) == S_OK && sourceType == 0);
    CHECK(source->GetOcclusionFactor(&readVector[0]) == S_OK && std::isfinite(readVector[0]));
    CHECK(source->GetAudibility(&readVector[0]) == S_OK && std::isfinite(readVector[0]));
    CHECK(FAILED(source->SetWavePosition(kBufferBytes + 1)));
    CHECK(source->SetWavePosition(16) == S_OK);
    CHECK(source->GetWavePosition(&sourceType) == S_OK && sourceType == 16);
    CHECK(FAILED(source->SetWaveTime(0)));
    CHECK(FAILED(source->GetWaveTime(&readVector[0])));
    CHECK(source->Rewind() == S_OK);
    CHECK(source->GetWavePosition(&sourceType) == S_OK && sourceType == 0);
    CHECK(FAILED(source->LoadWaveFile(const_cast<char*>("/nonexistent-cse-a3d-wave"))));
    CHECK(FAILED(source->LoadWaveData(nullptr, 0)));
    CHECK(source->SetGain(0.5f) == S_OK);
    float gain = 0;
    CHECK(source->GetGain(&gain) == S_OK && gain == 0.5f);
    CHECK(source->Play(A3D_LOOPED) == S_OK);
    CHECK(api->Flush() == S_OK);
    CHECK(module.unload() == S_FALSE);
    api->Release();
    CHECK(module.unload() == S_FALSE);
    CHECK(source->Stop() == S_OK);
    CHECK(FAILED(raw->SetPanValues(0, nullptr)));
    CHECK(FAILED(raw->GetPanValues(0, nullptr)));
    CHECK(FAILED(raw->SetWaveEvent(0, nullptr)));
    CHECK(FAILED(raw->ClearWaveEvents()));
    CHECK(FAILED(raw->SetTransformMode(0)));
    DWORD mode = 0;
    CHECK(FAILED(raw->GetTransformMode(&mode)));
    CHECK(FAILED(raw->SetReflectionDelayScale(1)));
    CHECK(FAILED(raw->GetReflectionDelayScale(&gain)));
    CHECK(FAILED(raw->SetReflectionGainScale(1)));
    CHECK(FAILED(raw->GetReflectionGainScale(&gain)));
    CHECK(source->FreeWaveData() == S_OK);
    CHECK(FAILED(source->FreeWaveData()));
    CHECK(source->GetWaveSize() == 0);
    CHECK(FAILED(source->SetWavePosition(16)));
    CHECK(FAILED(source->GetWavePosition(&sourceType)));
    CHECK(FAILED(source->Rewind()));
    source->Release();
    for (bool stream : {false, true}) {
        auto* sound = Provider(module);
        auto* buffer = Buffer(sound, stream);
        WritePcm(buffer);
        auto* notify = Query<IA3dNativeNotifyV1>(buffer, IID_IA3dNativeNotifyV1);
        auto* crossing = module.Event();
        auto* stop = module.Event();
        A3dNativeNotificationPositionV1 positions[]{{64, 0, crossing},
            {A3D_NATIVE_NOTIFICATION_STOP, 0, stop}};
        CHECK(notify->SetNotificationPositions(2, positions) == S_OK);
        CHECK(buffer->Play(0, 0, DSBPLAY_LOOPING) == S_OK);
        A3dNativeWaitStatusV1 status = A3dNativeWaitStatusV1::Invalid;
        CHECK(crossing->Wait(2000, nullptr, &status) == S_OK);
        CHECK(status == A3dNativeWaitStatusV1::Signaled);
        CHECK(buffer->Stop() == S_OK);
        CHECK(stop->Wait(2000, nullptr, &status) == S_OK && status == A3dNativeWaitStatusV1::Signaled);
        CHECK(notify->SetNotificationPositions(0, nullptr) == S_OK);
        crossing->Release();
        stop->Release();
        notify->Release();
        buffer->Release();
        sound->Release();
    }
}

void HeldInterfaces(Module& module) {
    auto* api = module.Create<IA3d3>(IID_IA3d3);
    auto* geometry = Query<IA3dGeomTarget>(api, IID_IA3dGeom);
    api->Release();
    CHECK(module.unload() == S_FALSE);
    geometry->Release();
    CHECK(module.unload() == S_OK);
    api = module.Create<IA3d3>(IID_IA3d3);
    geometry = Query<IA3dGeomTarget>(api, IID_IA3dGeom);
    IA3dMaterial* material = nullptr;
    CHECK(geometry->NewMaterial(&material) == S_OK);
    float defaultAmount = -1;
    float defaultHighFrequency = -1;
    CHECK(material->GetReflectance(&defaultAmount, &defaultHighFrequency) == S_OK);
    CHECK(defaultAmount == 1 && defaultHighFrequency == 1);
    CHECK(material->GetTransmittance(&defaultAmount, &defaultHighFrequency) == S_OK);
    CHECK(defaultAmount == 0 && defaultHighFrequency == 0);
    void* unsupported = reinterpret_cast<void*>(1);
    CHECK(material->QueryInterface(unknownIid, &unsupported) == E_NOINTERFACE && !unsupported);
    Canonical(material, material);
    CHECK(FAILED(material->SelectPreset(0)));
    DWORD preset = 0;
    CHECK(material->GetClosestPreset(&preset) == S_OK && preset == UINT32_MAX);
    CHECK(material->SetReflectance(0.5f, 0.75f) == S_OK);
    float amount = 0;
    float highFrequency = 0;
    CHECK(material->GetReflectance(&amount, &highFrequency) == S_OK);
    CHECK(amount == 0.5f && highFrequency == 0.75f);
    CHECK(material->SetTransmittance(0.25f, 0.5f) == S_OK);
    CHECK(material->GetTransmittance(&amount, &highFrequency) == S_OK);
    CHECK(amount == 0.25f && highFrequency == 0.5f);
    CHECK(material->SetNameID(const_cast<char*>("module material")) == S_OK);
    constexpr std::size_t kMaterialNameBytes = 256;
    char name[kMaterialNameBytes]{};
    CHECK(material->GetNameID(name, sizeof(name)) == S_OK && std::strcmp(name, "module material") == 0);
    CHECK(material->SetNameID(nullptr) == E_INVALIDARG);
    char fullName[kMaterialNameBytes + 1];
    std::memset(fullName, 'n', sizeof(fullName));
    fullName[kMaterialNameBytes - 1] = '\0';
    CHECK(material->SetNameID(fullName) == S_OK);
    CHECK(material->GetNameID(name, sizeof(name)) == S_OK);
    CHECK(std::strlen(name) == kMaterialNameBytes - 1);
    fullName[kMaterialNameBytes - 1] = 'n';
    fullName[kMaterialNameBytes] = '\0';
    CHECK(material->SetNameID(fullName) == E_INVALIDARG);
    CHECK(material->GetNameID(name, sizeof(name)) == S_OK);
    CHECK(std::strlen(name) == kMaterialNameBytes - 1);
    CHECK(material->SetNameID(const_cast<char*>("module material")) == S_OK);
    CHECK(material->GetNameID(name, sizeof(name)) == S_OK && std::strcmp(name, "module material") == 0);
    CHECK(FAILED(material->Load(name)));
    CHECK(FAILED(material->Save(name)));
    CHECK(FAILED(material->UnSerialize(nullptr, 0)));
    void* serialized = nullptr;
    UINT serializedBytes = 0;
    CHECK(FAILED(material->Serialize(&serialized, &serializedBytes)));
    IA3dMaterial* duplicate = nullptr;
    CHECK(FAILED(material->Duplicate(&duplicate)));
    geometry->Release();
    api->Release();
    CHECK(module.unload() == S_FALSE);
    material->Release();
    CHECK(module.unload() == S_OK);
    api = module.Create<IA3d3>(IID_IA3d3);
    geometry = Query<IA3dGeomTarget>(api, IID_IA3dGeom);
    IA3dList* list = nullptr;
    CHECK(geometry->NewList(&list) == S_OK);
    list->AddRef();
    CHECK(list->Release() == 1);
    geometry->Release();
    api->Release();
    CHECK(module.unload() == S_FALSE);
    CHECK(list->Begin() == S_OK);
    CHECK(SUCCEEDED(list->End()));
    CHECK(SUCCEEDED(list->Call()));
    CHECK(FAILED(list->EnableBoundingVol()));
    CHECK(list->Release() == 0);
    CHECK(module.unload() == S_OK);
    for (bool stream : {false, true}) {
        auto* sound = Provider(module);
        auto* buffer = Buffer(sound, stream);
        sound->Release();
        CHECK(module.unload() == S_FALSE);
        auto* notify = Query<IA3dNativeNotifyV1>(buffer, IID_IA3dNativeNotifyV1);
        buffer->Release();
        CHECK(module.unload() == S_FALSE);
        notify->Release();
        CHECK(module.unload() == S_OK);
    }
}

class ForeignEvent final : public IA3dNativeEventV1 {
public:
    unsigned calls = 0;
    HRESULT QueryInterface(REFIID, void**) override {
        ++calls;
        return E_NOINTERFACE;
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

void NotificationAdmission(Module& module, const char* foreignPath) {
    static_assert(sizeof(A3dNativeEventDescV1) == 16);
    static_assert(sizeof(A3dNativeNotificationCapsV1) == 32);
    static_assert(sizeof(A3dNativeNotificationPositionV1) == 16);
    static_assert(offsetof(A3dNativeNotificationPositionV1, event) == 8);
    CHECK(IID_IA3dNativeEventV1.Data1 == 0xec05e92f);
    CHECK(IID_IA3dNativeNotifyV1.Data1 == 0x37efe390);
    RejectAbi(module);
    CHECK(foreignPath && *foreignPath);
    Module foreignModule(foreignPath);
    CHECK(foreignModule.handle != module.handle && foreignModule.event != module.event);
    auto* foreignModuleEvent = foreignModule.Event();
    auto* event = module.Event();
    CHECK(module.unload() == S_FALSE);
    auto* view = Query<IA3dNativeEventV1>(event, IID_IA3dNativeEventV1);
    Canonical(event, view);
    view->Release();
    A3dNativeEventDescV1 invalid{sizeof(invalid), 2, 0, 0};
    IA3dNativeEventV1* rejected = reinterpret_cast<IA3dNativeEventV1*>(1);
    CHECK(module.event(&invalid, &rejected) == E_INVALIDARG && !rejected);
    invalid.version = 1;
    invalid.structureBytes = 0;
    CHECK(module.event(&invalid, &rejected) == E_INVALIDARG && !rejected);
    auto* sound = Provider(module);
    for (bool stream : {false, true}) {
        auto* buffer = Buffer(sound, stream);
        auto* notify = Query<IA3dNativeNotifyV1>(buffer, IID_IA3dNativeNotifyV1);
        Canonical(buffer, notify);
        void* legacy = reinterpret_cast<void*>(1);
        CHECK(buffer->QueryInterface(IID_IDirectSoundNotify, &legacy) == E_NOINTERFACE && !legacy);
        A3dNativeNotificationCapsV1 caps{sizeof(caps), 1, 0, 0, 0, {0, 0, 0}};
        CHECK(notify->GetNotificationCaps(&caps) == S_OK);
        CHECK(caps.flags == (A3D_NATIVE_NOTIFY_BYTE_OFFSETS | A3D_NATIVE_NOTIFY_STOP | A3D_NATIVE_NOTIFY_WRAP |
            A3D_NATIVE_NOTIFY_ATOMIC_REPLACE | A3D_NATIVE_NOTIFY_RETAINED_EVENTS));
        CHECK(caps.maximumPositions == A3D_NATIVE_NOTIFICATION_MAX_POSITIONS);
        CHECK(caps.cursorKind == (stream ? A3D_NATIVE_CURSOR_STREAM_SUBMITTED : A3D_NATIVE_CURSOR_RENDER_CONSUMED));
        ForeignEvent foreign;
        A3dNativeNotificationPositionV1 position{0, 0, &foreign};
        CHECK(notify->SetNotificationPositions(1, &position) == E_INVALIDARG);
        CHECK(foreign.calls == 0);
        position.event = foreignModuleEvent;
        CHECK(notify->SetNotificationPositions(1, &position) == E_INVALIDARG);
        CHECK(foreignModule.events().retainedStates == 0);
        position.event = reinterpret_cast<IA3dNativeEventV1*>(1);
        CHECK(notify->SetNotificationPositions(1, &position) == E_INVALIDARG);
        position.event = event;
        position.reserved = 1;
        CHECK(notify->SetNotificationPositions(1, &position) == E_INVALIDARG);
        position.reserved = 0;
        CHECK(notify->SetNotificationPositions(1, &position) == S_OK);
        CHECK(notify->SetNotificationPositions(0, nullptr) == S_OK);
        notify->Release();
        buffer->Release();
    }
    sound->Release();
    event->Release();
    foreignModuleEvent->Release();
}

void NotificationBusy(Module& module) {
    auto* event = module.Event(); // No provider exists; event alone pins the module.
    CHECK(module.unload() == S_FALSE);
    auto* cancel = module.Event(true);
    auto* sound = Provider(module);
    auto* buffer = Buffer(sound);
    auto* notify = Query<IA3dNativeNotifyV1>(buffer, IID_IA3dNativeNotifyV1);
    A3dNativeNotificationPositionV1 position{A3D_NATIVE_NOTIFICATION_STOP, 0, event};
    CHECK(notify->SetNotificationPositions(1, &position) == S_OK);
    CHECK(module.events().retainedStates == 1);
    event->Release(); // Only the registration holds this event.
    sound->Release();
    CHECK(module.unload() == S_FALSE);
    event->AddRef(); // Still valid via the retained registration.
    std::atomic<bool> entering{false};
    std::atomic<bool> returned{false};
    A3dNativeWaitStatusV1 result = A3dNativeWaitStatusV1::Invalid;
    std::thread waiter([&] {
        entering = true;
        CHECK(event->Wait(A3D_NATIVE_WAIT_INFINITE, cancel, &result) == S_OK);
        returned = true;
    });
    while (!entering.load()) {
        std::this_thread::yield();
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!module.events().waits && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }
    CHECK(module.events().waits == 1);
    CHECK(!returned.load());
    CHECK(module.unload() == S_FALSE);
    CHECK(notify->SetNotificationPositions(0, nullptr) == S_OK);
    notify->Release();
    buffer->Release();
    CHECK(module.unload() == S_FALSE);
    CHECK(cancel->Signal() == S_OK);
    waiter.join();
    CHECK(result == A3dNativeWaitStatusV1::Cancelled);
    CHECK(event->Close() == S_OK);
    event->Release();
    cancel->Release();
    CHECK(module.unload() == S_OK);
    // Exercise the separate close wake-up with an infinite waiter and host lease.
    event = module.Event();
    entering = false;
    returned = false;
    std::thread closeWaiter([&] {
        entering = true;
        CHECK(event->Wait(A3D_NATIVE_WAIT_INFINITE, nullptr, &result) == S_OK);
        returned = true;
    });
    while (!entering.load()) {
        std::this_thread::yield();
    }
    CHECK(event->Close() == S_OK);
    closeWaiter.join();
    CHECK(result == A3dNativeWaitStatusV1::Closed);
    event->Release();
}

void CallableMatrix(Module& module) {
    for (bool geometryFirst : {false, true}) {
        auto* api = module.Create<IA3d3>(IID_IA3d3);
        IA3dGeomTarget* geometry = nullptr;
        if (geometryFirst) {
            geometry = Query<IA3dGeomTarget>(api, IID_IA3dGeom);
        }
        CHECK(api->Init(nullptr, kDirectFeature, 0) == S_OK);
        if (!geometry) {
            geometry = Query<IA3dGeomTarget>(api, IID_IA3dGeom);
        }
        CHECK(geometry->IsEnabled(kDirectFeature));
        CHECK(!geometry->IsEnabled(A3D_DIRECT_PATH_GENERIC));
        CHECK(geometry->Enable(A3D_1ST_REFLECTIONS) == A3DERROR_FEATURE_NOT_SUPPORTED);
        CHECK(FAILED(geometry->SetReflectionMode(A3D_FASTEST)));
        CHECK(FAILED(geometry->SetOcclusionMode(A3D_FASTEST)));
        CHECK(geometry->Disable(kDirectFeature) == S_OK);
        CHECK(!geometry->IsEnabled(kDirectFeature));
        CHECK(geometry->Enable(kDirectFeature) == S_OK);
        CHECK(api->SetOutputGain(0.25f) == S_OK);
        float value = 0;
        CHECK(api->GetOutputGain(&value) == S_OK && value == 0.25f);
        DWORD mode = 0;
        CHECK(api->SetResourceManagerMode(A3D_RESOURCE_MODE_OFF) == S_OK);
        CHECK(api->GetResourceManagerMode(&mode) == S_OK && mode == A3D_RESOURCE_MODE_OFF);
        CHECK(FAILED(api->SetHFAbsorbFactor(0.5f)));
        CHECK(FAILED(api->GetHFAbsorbFactor(&value)));
        DWORD front = 0;
        DWORD back = 0;
        CHECK(FAILED(api->GetOutputMode(&front, &back, &mode)));
        CHECK(FAILED(api->SetOutputMode(0, 0, 0)));
        CHECK(FAILED(api->SetCoordinateSystem(0)));
        CHECK(api->GetCoordinateSystem(&mode) == S_OK && mode == A3D_RIGHT_HANDED_CS);
        CHECK(FAILED(api->SetCooperativeLevel(nullptr, 0)));
        CHECK(api->GetCooperativeLevel(&mode) == S_OK && mode == A3D_CL_NORMAL);
        auto* publicGeometry = reinterpret_cast<IA3dGeom*>(geometry);
        CHECK(FAILED(publicGeometry->SetRenderMode(0)));
        CHECK(FAILED(publicGeometry->GetRenderMode(&mode)));
        CHECK(FAILED(publicGeometry->SetPolygonBloatFactor(1)));
        CHECK(FAILED(publicGeometry->GetPolygonBloatFactor(&value)));
        CHECK(FAILED(publicGeometry->SetReflectionUpdateInterval(1)));
        CHECK(FAILED(publicGeometry->GetReflectionUpdateInterval(&mode)));
        CHECK(FAILED(publicGeometry->SetOcclusionUpdateInterval(1)));
        CHECK(FAILED(publicGeometry->GetOcclusionUpdateInterval(&mode)));
        CHECK(api->SetMaxReflectionDelayTime(0.2f) == S_OK);
        CHECK(api->GetMaxReflectionDelayTime(&value) == S_OK && value == 0.2f);
        CHECK(FAILED(api->SetNumFallbackSources(4)));
        CHECK(FAILED(api->GetNumFallbackSources(&mode)));
        CHECK(FAILED(api->RegisterVersion(20)));
        A3DCAPS_SOFTWARE software{};
        software.dwSize = sizeof(software);
        A3DCAPS_HARDWARE hardware{};
        hardware.dwSize = sizeof(hardware);
        CHECK(FAILED(api->GetSoftwareCaps(&software)));
        CHECK(FAILED(api->GetHardwareCaps(&hardware)));
        CHECK(api->Compat(1008, 0) == S_OK); // Retained named legacy no-operation opcode.
        CHECK(api->Compat(UINT32_MAX, 0) == E_INVALIDARG);
        CHECK(api->Clear() == S_OK);
        CHECK(api->Flush() == S_OK);
        geometry->Release();
        api->Release();
    }
    auto* initializedSound = module.Create<IDirectSound>(IID_IDirectSound);
    CHECK(initializedSound->Initialize(nullptr) == S_OK);
    CHECK(FAILED(initializedSound->Initialize(nullptr)));
    initializedSound->Release();
    auto* sound = Provider(module);
    DSCAPS caps{};
    caps.dwSize = sizeof(caps);
    CHECK(sound->GetCaps(&caps) == S_OK);
    CHECK(!caps.dwMaxHwMixingAllBuffers && !caps.dwMaxHw3DAllBuffers && !caps.dwTotalHwMemBytes);
    CHECK(FAILED(sound->Compact()));
    CHECK(FAILED(sound->SetCooperativeLevel(nullptr, 0)));
    DWORD config = 0;
    CHECK(FAILED(sound->GetSpeakerConfig(&config)));
    CHECK(FAILED(sound->SetSpeakerConfig(0)));
    for (bool stream : {false, true}) {
        auto* buffer = Buffer(sound, stream);
        WritePcm(buffer);
        auto format = Format();
        CHECK(FAILED(buffer->SetFormat(&format)));
        CHECK(FAILED(buffer->Initialize(sound, nullptr)));
        CHECK(stream ? buffer->Restore() == S_OK : FAILED(buffer->Restore()));
        WAVEFORMATEX read{};
        DWORD written = 0;
        CHECK(buffer->GetFormat(&read, sizeof(read), &written) == S_OK);
        CHECK(written == sizeof(read) && read.nSamplesPerSec == kSampleRate && read.nChannels == 1);
        CHECK(buffer->SetVolume(-1000) == S_OK);
        LONG scalar = 0;
        CHECK(buffer->GetVolume(&scalar) == S_OK && scalar == -1000);
        CHECK(buffer->SetPan(500) == S_OK);
        CHECK(buffer->GetPan(&scalar) == S_OK && scalar == 500);
        CHECK(buffer->SetFrequency(24000) == S_OK);
        DWORD frequency = 0;
        CHECK(buffer->GetFrequency(&frequency) == S_OK && frequency == 24000);
        auto* spatial = Query<IDirectSound3DBuffer>(buffer, IID_IDirectSound3DBuffer);
        CHECK(spatial->SetPosition(1, 2, 3, DS3D_IMMEDIATE) == S_OK);
        D3DVECTOR position{};
        CHECK(spatial->GetPosition(&position) == S_OK && position.x == 1 && position.y == 2 && position.z == 3);
        CHECK(spatial->SetVelocity(4, 5, 6, DS3D_IMMEDIATE) == S_OK);
        CHECK(spatial->GetVelocity(&position) == S_OK && position.x == 4 && position.z == 6);
        CHECK(spatial->SetConeAngles(60, 120, DS3D_IMMEDIATE) == S_OK);
        DWORD inside = 0;
        DWORD outside = 0;
        CHECK(spatial->GetConeAngles(&inside, &outside) == S_OK && inside == 60 && outside == 120);
        CHECK(spatial->SetConeOrientation(0, 0, 1, DS3D_IMMEDIATE) == S_OK);
        CHECK(spatial->GetConeOrientation(&position) == S_OK && position.z == 1);
        CHECK(spatial->SetConeOutsideVolume(-2000, DS3D_IMMEDIATE) == S_OK);
        CHECK(spatial->GetConeOutsideVolume(&scalar) == S_OK && scalar == -2000);
        float distance = 0;
        CHECK(spatial->SetMinDistance(2, DS3D_IMMEDIATE) == S_OK);
        CHECK(spatial->GetMinDistance(&distance) == S_OK && distance == 2);
        CHECK(spatial->SetMaxDistance(200, DS3D_IMMEDIATE) == S_OK);
        CHECK(spatial->GetMaxDistance(&distance) == S_OK && distance == 200);
        CHECK(spatial->SetMode(DS3DMODE_NORMAL, DS3D_IMMEDIATE) == S_OK);
        CHECK(spatial->GetMode(&frequency) == S_OK && frequency == DS3DMODE_NORMAL);
        DS3DBUFFER parameters{};
        parameters.dwSize = sizeof(parameters);
        CHECK(spatial->GetAllParameters(&parameters) == S_OK && parameters.vPosition.x == 1);
        CHECK(spatial->SetAllParameters(&parameters, DS3D_IMMEDIATE) == S_OK);
        CHECK(FAILED(spatial->SetAllParameters(&parameters, DS3D_DEFERRED)));
        spatial->Release();
        DSBCAPS bufferCaps{};
        bufferCaps.dwSize = sizeof(bufferCaps);
        CHECK(buffer->GetCaps(&bufferCaps) == S_OK && bufferCaps.dwBufferBytes == kBufferBytes);
        CHECK((bufferCaps.dwFlags & DSBCAPS_LOCSOFTWARE) && !(bufferCaps.dwFlags & DSBCAPS_LOCHARDWARE));
        CHECK(buffer->SetCurrentPosition(16) == S_OK);
        DWORD playCursor = 0;
        DWORD writeCursor = 0;
        CHECK(buffer->GetCurrentPosition(&playCursor, &writeCursor) == S_OK);
        CHECK(playCursor < kBufferBytes && writeCursor < kBufferBytes);
        CHECK(buffer->Play(0, 0, DSBPLAY_LOOPING) == S_OK);
        DWORD status = 0;
        CHECK(buffer->GetStatus(&status) == S_OK && (status & DSBSTATUS_PLAYING));
        CHECK(buffer->Stop() == S_OK);
        CHECK(buffer->GetStatus(&status) == S_OK && !(status & DSBSTATUS_PLAYING));
        buffer->Release();
    }
    sound->Release();
}
}

int main(int argc, char** argv) {
    CHECK(argc == 4);
    CHECK(std::getenv("SDL_AUDIO_DRIVER") && std::strcmp(std::getenv("SDL_AUDIO_DRIVER"), "dummy") == 0);
    const char* name = argv[2];
    if (std::strcmp(name, "repeated_load_unload") == 0) {
        constexpr unsigned kUnloadCycles = 12;
        for (unsigned i = 0; i < kUnloadCycles; ++i) {
            {
                Module module(argv[1]);
                auto* api = module.Create<IA3d3>(IID_IA3d3);
                api->Release();
                auto* event = module.Event();
                event->Release();
            }
            // Prove removal from the loader, not merely a balanced handle count.
            void* resident = dlopen(argv[1], RTLD_NOW | RTLD_NOLOAD);
            if (resident) {
                dlclose(resident);
            }
            CHECK(!resident);
        }
    } else {
        Module module(argv[1]);
        if (std::strcmp(name, "reject_abi_before_factory") == 0) {
            RejectAbi(module);
        } else if (std::strcmp(name, "class_iid_aggregation") == 0) {
            ClassIidAggregation(module);
        } else if (std::strcmp(name, "canonical_unknown") == 0) {
            CanonicalUnknown(module);
        } else if (std::strcmp(name, "qi_failure_cleanup") == 0) {
            QiFailureCleanup(module);
        } else if (std::strcmp(name, "factory_lock_balance") == 0) {
            FactoryLocks(module);
        } else if (std::strcmp(name, "source_buffer_playback") == 0) {
            SourcePlayback(module);
        } else if (std::strcmp(name, "held_interface_unload_refusal") == 0) {
            HeldInterfaces(module);
        } else if (std::strcmp(name, "notification_public_admission") == 0) {
            NotificationAdmission(module, argv[3]);
        } else if (std::strcmp(name, "notification_busy_unload") == 0) {
            NotificationBusy(module);
        } else if (std::strcmp(name, "callable_operation_matrix") == 0) {
            CallableMatrix(module);
        } else {
            CHECK(false);
        }
    }
    std::printf("PASS %s\n", name);
}
