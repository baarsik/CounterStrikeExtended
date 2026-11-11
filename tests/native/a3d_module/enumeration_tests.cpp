#include "enumeration_caller.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <new>
#include <string>
#include <thread>
#include <vector>

#define CHECK(condition) do { if (!(condition)) { \
    std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); std::exit(1); } } while (false)

namespace {
using independent::Enumerator;
using independent::DeviceInfo;
constexpr DWORD kRecordBytes = 32;
constexpr DWORD kDirectFeature = 0x08;
constexpr unsigned kAudio = 0x10;
constexpr DWORD kPcmBytes = 4096;
constexpr DWORD kSampleRate = 22050;
constexpr unsigned char kCanary = 0xa5;
using Owners = std::array<std::size_t, 14> (*)();
using GetClass = HRESULT (*)(REFCLSID, REFIID, void**);
using Unload = HRESULT (*)();

struct Module {
    void* handle;
    std::string path;
    cse::abi::GetModuleAbi describe;
    independent::Factory factory;
    GetClass getClass;
    Unload unload;
    Owners owners;
    unsigned audioState = 0;

    explicit Module(const char* filename) : path(filename) {
        handle = dlopen(filename, RTLD_NOW | RTLD_LOCAL);
        if (!handle) {
            std::fprintf(stderr, "%s\n", dlerror());
        }
        CHECK(handle);
        describe = reinterpret_cast<cse::abi::GetModuleAbi>(dlsym(handle, "CseGetModuleAbi"));
        CHECK(describe);
        CHECK(cse::abi::Check(ExpectedA3dEnumerationDescriptor(), describe()) ==
            cse::abi::Compatibility::Compatible);
        factory = reinterpret_cast<independent::Factory>(dlsym(handle, independent::FactoryName));
        CHECK(factory && "missing public enumeration export; no factory or virtual calls made");
        getClass = reinterpret_cast<GetClass>(dlsym(handle, "DllGetClassObject"));
        unload = reinterpret_cast<Unload>(dlsym(handle, "DllCanUnloadNow"));
        owners = reinterpret_cast<Owners>(dlsym(handle, "_Z15A3dNativeOwnersv"));
        CHECK(getClass && unload && owners);
        CHECK(unload() == S_OK);
        Symbol<void (*)()>("SDL_SetMainReady")();
        audioState = Symbol<unsigned (*)(unsigned)>("SDL_WasInit")(kAudio);
    }
    template<class T> T Symbol(const char* name) {
        auto result = reinterpret_cast<T>(dlsym(handle, name));
        CHECK(result);
        return result;
    }
    Enumerator* Create() {
        Enumerator* result = nullptr;
        CHECK(factory(&result) == S_OK && result);
        return result;
    }
    ~Module() {
        for (auto count : owners()) {
            CHECK(count == 0);
        }
        CHECK(unload() == S_OK);
        CHECK(Symbol<unsigned (*)(unsigned)>("SDL_WasInit")(kAudio) == audioState);
        CHECK(dlclose(handle) == 0);
        void* retained = dlopen(path.c_str(), RTLD_NOW | RTLD_NOLOAD);
        CHECK(retained == nullptr);
        std::printf("image absent after dlclose: %s\n", path.c_str());
    }
};

// This loader does not pre-admit the A3D image. Establish host SDL thread identity
// through the independently held SDL library before testing any candidate gate.
class AdmissionImage {
public:
    explicit AdmissionImage(const char* filename) : path_(filename) {
        const std::string sdlPath = path_.substr(0, path_.find_last_of('/') + 1) + "libSDL3.0.dylib";
        sdl_ = dlopen(sdlPath.c_str(), RTLD_NOW | RTLD_LOCAL);
        CHECK(sdl_);
        auto mainReady = reinterpret_cast<void (*)()>(dlsym(sdl_, "SDL_SetMainReady"));
        audioState_ = reinterpret_cast<unsigned (*)(unsigned)>(dlsym(sdl_, "SDL_WasInit"));
        CHECK(mainReady && audioState_);
        mainReady();
        initialAudio_ = audioState_(kAudio);
        image = dlopen(filename, RTLD_NOW | RTLD_LOCAL);
        CHECK(image);
        describe = reinterpret_cast<cse::abi::GetModuleAbi>(dlsym(image, "CseGetModuleAbi"));
        CHECK(describe);
    }
    ~AdmissionImage() {
        // Independent cleanup witness after the tested candidate path has finished.
        // Only now admit the actual trusted fixture for diagnostic export access.
        CHECK(cse::abi::Check(ExpectedA3dEnumerationDescriptor(), describe()) ==
            cse::abi::Compatibility::Compatible);
        auto owners = reinterpret_cast<Owners>(dlsym(image, "_Z15A3dNativeOwnersv"));
        auto unload = reinterpret_cast<Unload>(dlsym(image, "DllCanUnloadNow"));
        CHECK(owners && unload);
        for (auto count : owners()) {
            CHECK(count == 0);
        }
        CHECK(unload() == S_OK && audioState_(kAudio) == initialAudio_);
        CHECK(dlclose(image) == 0);
        CHECK(dlopen(path_.c_str(), RTLD_NOW | RTLD_NOLOAD) == nullptr);
        CHECK(dlclose(sdl_) == 0);
        std::puts("admission fixture clean: owners=0 audio=baseline unload=S_OK image=absent");
    }
    void* image = nullptr;
    cse::abi::GetModuleAbi describe = nullptr;
private:
    std::string path_;
    void* sdl_ = nullptr;
    unsigned (*audioState_)(unsigned) = nullptr;
    unsigned initialAudio_ = 0;
};

struct DispatchCounts {
    unsigned factory = 0;
    unsigned virtualCalls = 0;
    unsigned beforeAdmission = 0;
};

// Raw function/object pointers never leave this boundary. Every actual factory
// and virtual dispatch (including null-output calls and cleanup) is observed here.
class AdmissionDispatch {
public:
    void BeginCandidate() {
        counts = {};
        admitted_ = false;
        factory_ = nullptr;
    }
    bool Resolve(void* image, const char* name) {
        factory_ = reinterpret_cast<independent::Factory>(dlsym(image, name));
        return factory_ != nullptr;
    }
    void Admit() {
        admitted_ = true;
    }
    HRESULT Create(bool nullOutput = false) {
        CHECK(factory_ && !object_);
        Observe(counts.factory);
        return factory_(nullOutput ? nullptr : &object_);
    }
    HRESULT Count(DWORD* count) {
        CHECK(object_);
        Observe(counts.virtualCalls);
        return object_->GetCount(count);
    }
    HRESULT Info(DeviceInfo* info, char* name, DWORD capacity, DWORD* required) {
        CHECK(object_);
        Observe(counts.virtualCalls);
        return object_->GetInfo(0, independent::Version, sizeof(*info), info, name, capacity, required);
    }
    void Release() {
        if (object_) {
            Observe(counts.virtualCalls);
            CHECK(object_->Release() == 0);
            object_ = nullptr;
        }
    }
    bool HasObject() const {
        return object_ != nullptr;
    }
    ~AdmissionDispatch() {
        CHECK(!object_); // Every exit must account for terminal release before reporting counts.
    }
    DispatchCounts counts;
private:
    void Observe(unsigned& kind) {
        ++kind;
        if (!admitted_) {
            ++counts.beforeAdmission;
        }
    }
    independent::Factory factory_ = nullptr;
    Enumerator* object_ = nullptr;
    bool admitted_ = false;
};

enum class AdmissionRegression { None, EarlyFactory, EarlyVirtual };

bool GuardedInvoke(void* image, const cse::abi::Descriptor& expected, const cse::abi::Descriptor* offered,
    const char* requiredExport, AdmissionDispatch& boundary,
    AdmissionRegression regression = AdmissionRegression::None, bool retainAnchor = false) {
    boundary.BeginCandidate();
    // Intentional test-only regressions execute real early dispatch through the
    // identical observed boundary. They cannot fabricate an accepted device token.
    if (regression == AdmissionRegression::EarlyFactory) {
        CHECK(boundary.Resolve(image, independent::FactoryName));
        CHECK(boundary.Create() == S_OK);
    } else if (regression == AdmissionRegression::EarlyVirtual) {
        DWORD count = 0;
        CHECK(boundary.Count(&count) == S_OK);
    }
    const bool accepted = cse::abi::Check(expected, offered) == cse::abi::Compatibility::Compatible &&
        boundary.Resolve(image, requiredExport);
    if (accepted) {
        boundary.Admit();
        CHECK(boundary.Create(true) == E_POINTER); // Direct null-output factory calls count too.
        CHECK(boundary.Create() == S_OK);
        DWORD count = 0;
        CHECK(boundary.Count(&count) == S_OK);
        if (count) {
            DeviceInfo info{};
            DWORD required = 0;
            CHECK(boundary.Info(&info, nullptr, 0, &required) == S_FALSE && required > 0);
            std::vector<char> label(required);
            CHECK(boundary.Info(&info, label.data(), required, &required) == S_OK);
            CHECK(info.structureBytes == sizeof(info) && info.version == independent::Version);
        }
    }
    if (!accepted || !retainAnchor) {
        boundary.Release();
    }
    return accepted;
}

bool RejectedWithoutCalls(const char* label, bool accepted, const DispatchCounts& counts) {
    const bool clean = !accepted && !counts.factory && !counts.virtualCalls && !counts.beforeAdmission;
    std::printf("candidate=%s accepted=%u factory=%u virtual=%u before_admission=%u zero_call_gate=%s\n",
        label, accepted, counts.factory, counts.virtualCalls, counts.beforeAdmission, clean ? "PASS" : "FAIL");
    return clean;
}

bool ProbeRejectedCandidate(void* context, const char* label, const cse::abi::Descriptor& expected,
    const cse::abi::Descriptor* offered, const char* requiredExport) {
    AdmissionDispatch boundary;
    const bool accepted = GuardedInvoke(context, expected, offered, requiredExport, boundary);
    return RejectedWithoutCalls(label, accepted, boundary.counts);
}

void PositiveAdmission(AdmissionImage& image, AdmissionDispatch& boundary, bool retainAnchor = false) {
    CHECK(GuardedInvoke(image.image, ExpectedA3dEnumerationDescriptor(), image.describe(),
        independent::FactoryName, boundary, AdmissionRegression::None, retainAnchor));
    std::printf("positive observed factory=%u virtual=%u before_admission=%u anchor=%u\n",
        boundary.counts.factory, boundary.counts.virtualCalls, boundary.counts.beforeAdmission, retainAnchor);
    CHECK(boundary.counts.factory > 0 && boundary.counts.virtualCalls > 0 && !boundary.counts.beforeAdmission);
}

int AdmissionCases(const char* filename, const std::string& name, const char* baselinePath) {
    constexpr int kDetectedEarlyDispatch = 71;
    AdmissionImage image(filename);
    AdmissionDispatch boundary;
    if (name == "enumeration_admission_early_factory" || name == "enumeration_admission_early_virtual") {
        const bool earlyVirtual = name == "enumeration_admission_early_virtual";
        if (earlyVirtual) {
            // Seed through a fully admitted, separately observed real factory invocation.
            // The main-thread anchor remains private until the rejected attempt cleans it.
            PositiveAdmission(image, boundary, true);
        }
        const bool accepted = GuardedInvoke(image.image, ExpectedA3dEnumerationDescriptor(), nullptr,
            independent::FactoryName, boundary,
            earlyVirtual ? AdmissionRegression::EarlyVirtual : AdmissionRegression::EarlyFactory);
        const bool gatePassed = RejectedWithoutCalls(name.c_str(), accepted, boundary.counts);
        return gatePassed ? 0 : kDetectedEarlyDispatch;
    }
    PositiveAdmission(image, boundary);
    if (name == "enumeration_admission_mutations") {
        CHECK(CheckEnumerationAdmissionMutations(*image.describe(), ProbeRejectedCandidate, image.image));
    } else if (name == "enumeration_baseline_rejection") {
        CHECK(baselinePath && baselinePath[0]);
        void* baseline = dlopen(baselinePath, RTLD_NOW | RTLD_LOCAL);
        CHECK(baseline);
        auto describe = reinterpret_cast<cse::abi::GetModuleAbi>(dlsym(baseline, "CseGetModuleAbi"));
        CHECK(describe);
        CHECK(ProbeRejectedCandidate(baseline, "new_caller_old_image", ExpectedA3dEnumerationDescriptor(),
            describe(), independent::FactoryName));
        CHECK(ProbeRejectedCandidate(image.image, "old_caller_new_image", *describe(), image.describe(),
            independent::FactoryName));
        CHECK(dlclose(baseline) == 0);
        CHECK(dlopen(baselinePath, RTLD_NOW | RTLD_NOLOAD) == nullptr);
    }
    return 0;
}

struct Copy {
    DeviceInfo info{};
    std::vector<char> name;
};
Copy Read(Enumerator* enumerator, DWORD index) {
    Copy copy;
    DWORD required = 0;
    std::memset(&copy.info, kCanary, sizeof(copy.info));
    CHECK(enumerator->GetInfo(index, independent::Version, sizeof(copy.info), &copy.info,
        nullptr, 0, &required) == S_FALSE);
    const DeviceInfo empty{};
    CHECK(std::memcmp(&copy.info, &empty, sizeof(empty)) == 0 && required >= 1);
    copy.name.resize(required);
    CHECK(enumerator->GetInfo(index, independent::Version, sizeof(copy.info), &copy.info,
        copy.name.data(), required, &required) == S_OK);
    CHECK(copy.info.structureBytes == kRecordBytes && copy.info.version == independent::Version);
    CHECK(copy.info.nameBytes == required && copy.info.reserved == 0 && copy.name.back() == 0);
    return copy;
}
DWORD Count(Enumerator* enumerator) {
    DWORD count = UINT32_MAX;
    CHECK(enumerator->GetCount(nullptr) == E_POINTER);
    CHECK(enumerator->GetCount(&count) == S_OK);
    return count;
}
void Identity(Enumerator* enumerator) {
    CHECK(enumerator->QueryInterface(IID_IUnknown, nullptr) == E_POINTER);
    IUnknown* unknown = nullptr;
    CHECK(enumerator->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&unknown)) == S_OK);
    CHECK(unknown == static_cast<IUnknown*>(enumerator));
    CHECK(unknown->Release() == 1);
    Enumerator* view = nullptr;
    CHECK(enumerator->QueryInterface(independent::EnumeratorIid, reinterpret_cast<void**>(&view)) == S_OK);
    CHECK(view == enumerator && view->Release() == 1);
    void* failure = enumerator;
    CHECK(enumerator->QueryInterface(independent::DeviceIid, &failure) == E_NOINTERFACE && !failure);
    CHECK(enumerator->AddRef() == 2 && enumerator->Release() == 1);
}
void Bounds(Enumerator* enumerator) {
    const DWORD count = Count(enumerator);
    for (DWORD index = 0; index < count; ++index) {
        const Copy full = Read(enumerator, index);
        const DWORD required = full.info.nameBytes;
        for (DWORD bytes : {0u, 1u, 31u, 32u, 33u}) {
            for (DWORD capacity : {0u, required - 1, required, required + 1}) {
                alignas(DeviceInfo) std::array<unsigned char, 48> storage;
                storage.fill(kCanary);
                auto* info = new (storage.data() + 4) DeviceInfo;
                std::vector<char> name(capacity + 2, static_cast<char>(kCanary));
                DWORD needed = UINT32_MAX;
                const HRESULT status = enumerator->GetInfo(index, 1, bytes, info, name.data() + 1,
                    capacity, &needed);
                CHECK(status == (bytes != kRecordBytes ? E_INVALIDARG : capacity < required ? S_FALSE : S_OK));
                CHECK(storage[0] == kCanary && storage[3] == kCanary);
                for (std::size_t offset = 4 + std::min(bytes, kRecordBytes); offset < storage.size(); ++offset) {
                    CHECK(storage[offset] == kCanary);
                }
                CHECK(static_cast<unsigned char>(name.front()) == kCanary);
                CHECK(static_cast<unsigned char>(name.back()) == kCanary);
                if (status != S_OK) {
                    for (DWORD offset = 0; offset < std::min(bytes, kRecordBytes); ++offset) {
                        CHECK(storage[4 + offset] == 0);
                    }
                    CHECK(needed == (bytes == kRecordBytes ? required : 0));
                    if (capacity) {
                        CHECK(name[1] == 0);
                    }
                } else {
                    CHECK(std::memcmp(info, &full.info, sizeof(*info)) == 0);
                    CHECK(std::memcmp(name.data() + 1, full.name.data(), required) == 0);
                    for (DWORD offset = required; offset < capacity; ++offset) {
                        CHECK(static_cast<unsigned char>(name[offset + 1]) == kCanary);
                    }
                }
            }
        }
        DeviceInfo info{};
        DWORD needed = UINT32_MAX;
        CHECK(enumerator->GetInfo(index, 1, kRecordBytes, nullptr, nullptr, 0, &needed) == E_POINTER);
        CHECK(needed == 0);
        CHECK(enumerator->GetInfo(index, 1, kRecordBytes, &info, nullptr, 0, nullptr) == E_POINTER);
        CHECK(enumerator->GetInfo(index, 1, kRecordBytes, &info, nullptr, 1, &needed) == E_POINTER);
        CHECK(enumerator->GetInfo(index, 2, kRecordBytes, &info, nullptr, 0, &needed) == E_INVALIDARG);
        CHECK(needed == 0);
        for (unsigned mask = 0; mask < 8; ++mask) {
            for (DWORD capacity : {0u, 1u}) {
                DeviceInfo record{};
                std::memset(&record, kCanary, sizeof(record));
                char label = static_cast<char>(kCanary);
                DWORD result = UINT32_MAX;
                DeviceInfo* recordPointer = (mask & 1) ? &record : nullptr;
                DWORD* requiredPointer = (mask & 2) ? &result : nullptr;
                char* labelPointer = (mask & 4) ? &label : nullptr;
                const bool invalid = !recordPointer || !requiredPointer || (!labelPointer && capacity);
                const HRESULT status = enumerator->GetInfo(index, 1, kRecordBytes, recordPointer,
                    labelPointer, capacity, requiredPointer);
                CHECK(status == (invalid ? E_POINTER : capacity < required ? S_FALSE : S_OK));
                if (requiredPointer && invalid) {
                    CHECK(result == 0);
                }
                if (recordPointer && status != S_OK) {
                    const DeviceInfo empty{};
                    CHECK(std::memcmp(recordPointer, &empty, sizeof(empty)) == 0);
                }
                if (labelPointer && capacity && status != S_OK) {
                    CHECK(label == 0);
                }
            }
        }
        const Copy repeated = Read(enumerator, index);
        CHECK(repeated.name == full.name && std::memcmp(&repeated.info, &full.info, sizeof(info)) == 0);
    }
    const std::array<std::pair<DWORD, DWORD>, 4> invalidRequests{{{count, 1}, {UINT32_MAX, 1}, {0, 0}, {0, 2}}};
    for (const auto& request : invalidRequests) {
        alignas(DeviceInfo) std::array<unsigned char, 40> storage;
        storage.fill(kCanary);
        auto* info = new (storage.data() + 4) DeviceInfo;
        std::array<char, 3> label;
        label.fill(static_cast<char>(kCanary));
        DWORD needed = 9;
        CHECK(enumerator->GetInfo(request.first, request.second, kRecordBytes, info,
            label.data() + 1, 1, &needed) == E_INVALIDARG);
        CHECK(needed == 0 && label[1] == 0);
        CHECK(static_cast<unsigned char>(label.front()) == kCanary &&
            static_cast<unsigned char>(label.back()) == kCanary);
        for (std::size_t index = 0; index < storage.size(); ++index) {
            CHECK(storage[index] == (index >= 4 && index < 4 + kRecordBytes ? 0 : kCanary));
        }
    }
}
IDirectSound* Provider(Module& module, const GUID& token, HRESULT expected = S_OK) {
    IClassFactory* factory = nullptr;
    CHECK(module.getClass(CLSID_A3dApi, IID_IClassFactory, reinterpret_cast<void**>(&factory)) == S_OK);
    IDirectSound* api = nullptr;
    CHECK(factory->CreateInstance(nullptr, IID_IDirectSound, reinterpret_cast<void**>(&api)) == S_OK);
    factory->Release();
    independent::Device* device = nullptr;
    CHECK(api->QueryInterface(independent::DeviceIid, reinterpret_cast<void**>(&device)) == S_OK);
    DWORD supported = UINT32_MAX;
    CHECK(device->InitializeDevice(&token, kDirectFeature, 1, &supported) == expected);
    CHECK(supported == (expected == S_OK ? kDirectFeature : 0));
    device->Release();
    return api;
}
IA3d3* SourceProvider(Module& module, GUID& token) {
    IClassFactory* factory = nullptr;
    CHECK(module.getClass(CLSID_A3dApi, IID_IClassFactory, reinterpret_cast<void**>(&factory)) == S_OK);
    IA3d3* api = nullptr;
    CHECK(factory->CreateInstance(nullptr, IID_IA3d3, reinterpret_cast<void**>(&api)) == S_OK);
    factory->Release();
    // CA3d does not expose the typed provider QI view. Its existing public Init
    // uses the same GUID and internally calls the native typed initialization path.
    CHECK(api->Init(&token, kDirectFeature, 0) == S_OK);
    return api;
}
void BufferPlayback(IDirectSound* sound) {
    WAVEFORMATEX format{WAVE_FORMAT_PCM, 1, kSampleRate, kSampleRate * 2, 2, 16, 0};
    DSBUFFERDESC descriptor{};
    descriptor.dwSize = sizeof(descriptor);
    descriptor.dwFlags = DSBCAPS_STATIC | DSBCAPS_CTRLVOLUME;
    descriptor.dwBufferBytes = kPcmBytes;
    descriptor.lpwfxFormat = &format;
    IDirectSoundBuffer* buffer = nullptr;
    CHECK(sound->CreateSoundBuffer(&descriptor, &buffer, nullptr) == S_OK);
    void* first = nullptr;
    void* second = nullptr;
    DWORD firstBytes = 0;
    DWORD secondBytes = 0;
    CHECK(buffer->Lock(0, kPcmBytes, &first, &firstBytes, &second, &secondBytes, 0) == S_OK);
    CHECK(firstBytes == kPcmBytes);
    std::memset(first, 0x25, firstBytes);
    CHECK(buffer->Unlock(first, firstBytes, second, secondBytes) == S_OK);
    CHECK(buffer->Play(0, 0, DSBPLAY_LOOPING) == S_OK);
    DWORD played = 0;
    DWORD written = 0;
    for (unsigned attempt = 0; attempt < 100 && !played; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        CHECK(buffer->GetCurrentPosition(&played, &written) == S_OK);
    }
    CHECK(played > 0);
    std::printf("explicit typed selected provider buffer advanced: %u bytes\n", played);
    CHECK(buffer->Stop() == S_OK);
    buffer->Release();
}
void Playback(IA3d3* api) {
    IA3dSource* raw = nullptr;
    CHECK(api->NewSource(0, &raw) == S_OK);
    auto* source = reinterpret_cast<IA3dSource202*>(raw);
    WAVEFORMATEX format{WAVE_FORMAT_PCM, 1, kSampleRate, kSampleRate * 2, 2, 16, 0};
    CHECK(source->SetWaveFormat(&format) == S_OK);
    CHECK(source->AllocateWaveData(kPcmBytes) == S_OK);
    void* first = nullptr;
    void* second = nullptr;
    DWORD firstBytes = 0;
    DWORD secondBytes = 0;
    CHECK(source->Lock(0, kPcmBytes, &first, &firstBytes, &second, &secondBytes, 0) == S_OK);
    CHECK(firstBytes == kPcmBytes);
    std::memset(first, 0x25, firstBytes);
    CHECK(source->Unlock(first, firstBytes, second, secondBytes) == S_OK);
    CHECK(source->Play(A3D_LOOPED) == S_OK);
    CHECK(api->Flush() == S_OK);
    DWORD position = 0;
    for (unsigned attempt = 0; attempt < 100 && !position; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        CHECK(source->GetWavePosition(&position) == S_OK);
    }
    CHECK(position > 0);
    std::printf("selected public token source playback advanced: %u bytes\n", position);
    CHECK(source->Stop() == S_OK);
    source->Release();
}
}

int main(int argc, char** argv) {
    CHECK(argc >= 3);
    CHECK(std::getenv("SDL_AUDIO_DRIVER") && std::strcmp(std::getenv("SDL_AUDIO_DRIVER"), "dummy") == 0);
    const std::string name = argv[2];
    if (name.rfind("enumeration_admission_", 0) == 0 || name == "enumeration_baseline_rejection") {
        return AdmissionCases(argv[1], name, argc >= 4 ? argv[3] : nullptr);
    }
    if (name == "repeated_enumerator_reload") {
        Copy previous;
        for (unsigned cycle = 0; cycle < 3; ++cycle) {
            Module current(argv[1]);
            auto* snapshot = current.Create();
            CHECK(Count(snapshot) > 0);
            const Copy selected = Read(snapshot, 0);
            if (cycle) {
                CHECK(std::memcmp(&previous.info.device, &selected.info.device, sizeof(GUID)) != 0 &&
                    "observed reload token difference is a test premise, not a uniqueness guarantee");
                auto* rejected = Provider(current, previous.info.device, E_INVALIDARG);
                rejected->Release();
            }
            auto* accepted = Provider(current, selected.info.device);
            accepted->Release();
            snapshot->Release();
            previous = selected;
        }
        return 0;
    }
    Module module(argv[1]);
    CHECK(module.factory(nullptr) == E_POINTER);
    if (name == "snapshot_failure_cleanup") {
        Enumerator* wrongThread = reinterpret_cast<Enumerator*>(1);
        HRESULT status = S_OK;
        std::thread worker([&] { status = module.factory(&wrongThread); });
        worker.join();
        CHECK(status == E_FAIL && wrongThread == nullptr && module.unload() == S_OK);
        return 0;
    }
    if (name.rfind("fault_", 0) == 0) {
        using Arm = void (*)(const char*);
        using Finish = unsigned (*)();
        auto arm = reinterpret_cast<Arm>(dlsym(RTLD_DEFAULT, "CseEnumerationFaultArm"));
        auto finish = reinterpret_cast<Finish>(dlsym(RTLD_DEFAULT, "CseEnumerationFaultFinish"));
        CHECK(arm && finish);
        arm(name.c_str() + 6);
        Enumerator* result = reinterpret_cast<Enumerator*>(1);
        const HRESULT status = module.factory(&result);
        if (name == "fault_empty" || name == "fault_long" || name == "fault_multibyte" || name == "fault_empty_name") {
            CHECK(status == S_OK && result);
            const DWORD count = Count(result);
            if (name == "fault_empty") {
                CHECK(count == 0);
            } else {
                CHECK(count > 0);
                const Copy copy = Read(result, 0);
                if (name == "fault_long") {
                    CHECK(copy.name.size() == 4097);
                } else if (name == "fault_multibyte") {
                    CHECK(std::string(copy.name.data()) == "Głośnik 🎧");
                } else {
                    CHECK(copy.name.size() == 1);
                }
            }
            Bounds(result);
            CHECK(result->Release() == 0);
        } else {
            CHECK(status == (name.rfind("fault_alloc", 0) == 0 ? E_OUTOFMEMORY : E_FAIL));
            CHECK(result == nullptr);
        }
        const unsigned observed = finish();
        CHECK(observed > 0);
        std::printf("labeled fault %s observed %u; cleanup complete\n", name.c_str(), observed);
        return 0;
    }
    if (name == "stale_foreign_tokens") {
        CHECK(argc >= 4 && argv[3][0]);
        auto* local = module.Create();
        CHECK(Count(local) > 0);
        const Copy localCopy = Read(local, 0);
        {
            Module foreign(argv[3]);
            CHECK(foreign.handle != module.handle && foreign.factory != module.factory);
            CHECK(foreign.Symbol<bool (*)(unsigned)>("SDL_InitSubSystem") ==
                module.Symbol<bool (*)(unsigned)>("SDL_InitSubSystem"));
            std::puts("foreign module is a distinct loaded image sharing the canonical SDL provider");
            auto* other = foreign.Create();
            CHECK(Count(other) > 0);
            const Copy foreignCopy = Read(other, 0);
            CHECK(std::memcmp(&localCopy.info.device, &foreignCopy.info.device, sizeof(GUID)) != 0);
            auto* api = Provider(module, foreignCopy.info.device, E_INVALIDARG);
            api->Release();
            other->Release();
        }
        local->Release();
        auto* refreshed = module.Create();
        CHECK(Count(refreshed) > 0);
        const Copy refreshedCopy = Read(refreshed, 0);
        CHECK(std::memcmp(&localCopy.info.device, &refreshedCopy.info.device, sizeof(GUID)) != 0);
        auto* api = Provider(module, localCopy.info.device, E_INVALIDARG);
        api->Release();
        refreshed->Release();
        return 0;
    }
    if (name == "shared_audio_ownership") {
        CHECK(module.Symbol<bool (*)(unsigned)>("SDL_InitSubSystem")(kAudio));
    }
    auto* enumerator = module.Create();
    Identity(enumerator);
    const DWORD count = Count(enumerator);
    std::printf("observed playback snapshot count: %u\n", count);
    CHECK(module.unload() == S_FALSE);
    if (name == "sized_output_validation") {
        Bounds(enumerator);
    } else if (name == "public_selected_initialize") {
        CHECK(count > 0);
        Copy selected = Read(enumerator, 0);
        auto* provider = Provider(module, selected.info.device);
        auto* api = SourceProvider(module, selected.info.device);
        enumerator->Release();
        enumerator = nullptr;
        BufferPlayback(provider);
        Playback(api);
        provider->Release();
        api->Release();
    } else if (name == "copied_record_name") {
        CHECK(count > 0);
        const Copy copy = Read(enumerator, 0);
        const auto nameCopy = copy.name;
        enumerator->Release();
        enumerator = nullptr;
        CHECK(copy.name == nameCopy && copy.info.structureBytes == kRecordBytes);
    } else if (name == "concurrent_snapshot_reads") {
        std::vector<std::thread> readers;
        for (unsigned reader = 0; reader < 4; ++reader) {
            enumerator->AddRef();
            readers.emplace_back([enumerator, count] {
                for (unsigned pass = 0; pass < 20; ++pass) {
                    CHECK(Count(enumerator) == count);
                    Enumerator* view = nullptr;
                    CHECK(enumerator->QueryInterface(independent::EnumeratorIid,
                        reinterpret_cast<void**>(&view)) == S_OK && view == enumerator);
                    view->Release();
                    for (DWORD index = 0; index < count; ++index) {
                        Read(enumerator, index);
                    }
                }
                enumerator->Release();
            });
        }
        for (auto& reader : readers) {
            reader.join();
        }
    } else if (name == "held_enumerator_unload") {
        auto* second = module.Create();
        CHECK(second->Release() == 0 && module.unload() == S_FALSE);
    } else if (name == "empty_snapshot") {
        // A genuine zero count is accepted; dummy's count is observed, never manufactured here.
        Bounds(enumerator);
    }
    if (enumerator) {
        CHECK(enumerator->Release() == 0);
    }
    if (name == "shared_audio_ownership") {
        CHECK((module.Symbol<unsigned (*)(unsigned)>("SDL_WasInit")(kAudio) & kAudio) != 0);
        module.Symbol<void (*)(unsigned)>("SDL_QuitSubSystem")(kAudio);
    }
    return 0;
}
