#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include "native_device_init.h"

#include "platform_dsound.h"

struct a3d_platform_buffer_desc_s;
using a3d_platform_buffer_desc_t = a3d_platform_buffer_desc_s;
#include "production_bodies.inc"

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "A3D device failure: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

struct ProviderState {
    ULONG references = 1;
    int destroys = 0;
    int nativeQueries = 0;
    int legacyQueries = 0;
    int nativeInitializations = 0;
    int legacyInitializations = 0;
    int defaultInitializations = 0;
    bool nativeAvailable = true;
    bool legacyAvailable = true;
    bool hasDevice = false;
    bool compatibility = false;
    HRESULT initializationResult = S_OK;
    GUID copiedDevice{};
    std::uintptr_t borrowedAddress = 0;
    DWORD features = 0;
    DWORD initializationMode = 0;
    DWORD legacyDeviceWord = 0;
};

// Explicit interface test double, not a native provider or renderer. Unselected
// DAL/DirectSound operations abort instead of supplying successful behavior.
class Provider final : public IDirectSound, public IA3dDal, public IA3dNativeDeviceV1 {
public:
    explicit Provider(ProviderState& state) : state(state) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override {
        *object = nullptr;
        if (std::memcmp(&iid, &IID_IA3dNativeDeviceV1, sizeof(iid)) == 0) {
            ++state.nativeQueries;
            if (!state.nativeAvailable) {
                return E_NOINTERFACE;
            }
            *object = static_cast<IA3dNativeDeviceV1*>(this);
        } else if (std::memcmp(&iid, &s_BackendInterfaceId, sizeof(iid)) == 0) {
            ++state.legacyQueries;
            if (!state.legacyAvailable) {
                return E_NOINTERFACE;
            }
            *object = static_cast<IA3dDal*>(this);
        } else {
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return ++state.references;
    }

    ULONG STDMETHODCALLTYPE Release() override {
        const auto remaining = --state.references;
        if (!remaining) {
            ++state.destroys;
            delete this;
        }
        return remaining;
    }

    HRESULT STDMETHODCALLTYPE InitializeDevice(const GUID* device, DWORD features,
        DWORD initializationMode, DWORD* supportedFeatures) override {
        ++state.nativeInitializations;
        RecordDevice(device);
        state.features = features;
        state.initializationMode = initializationMode;
        *supportedFeatures = A3D_1ST_REFLECTIONS;
        return state.initializationResult;
    }

    HRESULT STDMETHODCALLTYPE Initialize(LPCGUID device) override {
        ++state.defaultInitializations;
        RecordDevice(device);
        return state.initializationResult;
    }

    HRESULT STDMETHODCALLTYPE InitializeEx(DWORD device, DWORD features, void* mode, void* supported) override {
        ++state.legacyInitializations;
        state.legacyDeviceWord = device;
        state.features = features;
        state.initializationMode = static_cast<DWORD>(reinterpret_cast<std::uintptr_t>(mode));
        *static_cast<DWORD*>(supported) = A3D_1ST_REFLECTIONS;
#if UINTPTR_MAX == UINT32_MAX
        RecordDevice(reinterpret_cast<const GUID*>(static_cast<std::uintptr_t>(device)));
#endif
        return state.initializationResult;
    }

    HRESULT STDMETHODCALLTYPE GetA3dCaps(void* value, void*) override {
        auto* caps = static_cast<a3d_backend_caps_t*>(value);
        std::memset(caps, 0, sizeof(*caps));
        caps->wCompatibilityId = state.compatibility ? A3D_BACKEND_COMPATIBILITY_ID : 0;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CreateSoundBufferEx(const a3d_platform_buffer_desc_t*, BYTE*,
        IDirectSoundBuffer**, IUnknown*) override {
        std::abort();
    }
    HRESULT STDMETHODCALLTYPE GetDriverInfo(void*, void*, void*, void*) override {
        std::abort();
    }
    HRESULT STDMETHODCALLTYPE GetDS(IDirectSound**) override {
        std::abort();
    }
    HRESULT STDMETHODCALLTYPE GetDSDriverDesc(void*, void*) override {
        std::abort();
    }
    HRESULT STDMETHODCALLTYPE QueryFunctionality(DWORD, void*) override {
        std::abort();
    }
    HRESULT STDMETHODCALLTYPE DeviceCompatibility5(void*, void*, void*) override {
        std::abort();
    }
    HRESULT STDMETHODCALLTYPE CreateSoundBuffer(LPCDSBUFFERDESC, IDirectSoundBuffer**, IUnknown*) override {
        std::abort();
    }
    HRESULT STDMETHODCALLTYPE GetCaps(LPDSCAPS) override {
        std::abort();
    }
    HRESULT STDMETHODCALLTYPE DuplicateSoundBuffer(IDirectSoundBuffer*, IDirectSoundBuffer**) override {
        std::abort();
    }
    HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND, DWORD) override {
        std::abort();
    }
    HRESULT STDMETHODCALLTYPE Compact() override {
        std::abort();
    }
    HRESULT STDMETHODCALLTYPE GetSpeakerConfig(LPDWORD) override {
        std::abort();
    }
    HRESULT STDMETHODCALLTYPE SetSpeakerConfig(DWORD) override {
        std::abort();
    }

private:
    void RecordDevice(const GUID* device) {
        state.hasDevice = device != nullptr;
        state.borrowedAddress = reinterpret_cast<std::uintptr_t>(device);
        if (device) {
            state.copiedDevice = *device;
        }
    }

    ProviderState& state;
};

static constexpr GUID testDevice = {0x89abcdef, 0x1234, 0x5678, {0x91, 0xa2, 0xb3, 0xc4, 0xd5, 0xe6, 0xf7, 0x08}};

static void Test(const std::string& mode) {
    static_assert(sizeof(GUID) == 16, "fixed Windows GUID content");
    Require(std::memcmp(&IID_IA3dNativeDeviceV1, &s_BackendInterfaceId, sizeof(IID)) != 0,
        "new versioned IID is distinct from legacy DWORD interface");
    ProviderState state;
    state.nativeAvailable = mode != "no-native" && mode != "fallback-failure";
    state.legacyAvailable = mode != "missing-dal";
    state.initializationResult = mode == "failure" || mode == "fallback-failure" ? E_FAIL : S_OK;
    state.compatibility = mode == "compatibility";
    CA3d object;
    object.SetupBackend(new Provider(state), nullptr, nullptr);
    constexpr DWORD requestedFeatures = 0x12345678;
    auto device = std::make_unique<GUID>(testDevice);
#if UINTPTR_MAX > UINT32_MAX
    Require(reinterpret_cast<std::uintptr_t>(device.get()) > UINT32_MAX, "actual GUID storage exceeds DWORD range");
#endif
    GUID* input = mode == "default" ? nullptr : device.get();
    const auto inputAddress = reinterpret_cast<std::uintptr_t>(input);
    const HRESULT result = object.Init(input, requestedFeatures, 0);
    std::printf("device=%p legacy_device_word=%08x native_calls=%d default_calls=%d\n", static_cast<void*>(input),
        static_cast<unsigned int>(state.legacyDeviceWord), state.nativeInitializations, state.defaultInitializations);
    const bool nativeWidth = sizeof(void*) > sizeof(DWORD);
    const bool expectedFailure = mode == "failure" || mode == "fallback-failure"
        || (nativeWidth && mode == "missing-dal");
    Require(result == (expectedFailure ? A3DERROR_FAILED_INIT : S_OK), "public initialization failure mapping");
    Require(object.m_dwRequestedFeatures == requestedFeatures, "requested feature word retained");
    if (!expectedFailure) {
        const bool typed = nativeWidth && mode != "no-native";
        Require(state.nativeInitializations == (typed ? 1 : 0), "native Init uses typed versioned interface");
        Require(state.legacyInitializations == (nativeWidth || !state.legacyAvailable ? 0 : 1),
            "native Init never calls legacy DWORD device API; Win32 preserves its original route");
        if (nativeWidth) {
            Require(state.nativeQueries == 1, "native versioned IID queried exactly once");
            Require(state.defaultInitializations == (mode == "no-native" ? 1 : 0), "missing native IID uses DirectSound fallback");
            Require(state.legacyQueries == (mode == "no-native" ? 0 : 1), "old IID is used only for non-init backend methods");
            Require(state.borrowedAddress != inputAddress || !input, "callee receives a bounded GUID value copy");
        }
        Require(state.hasDevice == (input != nullptr), "null device selects default without fabricated GUID");
        if (input) {
            Require(std::memcmp(&state.copiedDevice, &testDevice, sizeof(GUID)) == 0, "complete GUID content retained");
        }
        device.reset();
        Require(!state.hasDevice || std::memcmp(&state.copiedDevice, &testDevice, sizeof(GUID)) == 0,
            "provider-owned content survives caller GUID storage release");
        if (mode == "repeat" && nativeWidth) {
            constexpr int initializationCycles = 64;
            for (int cycle = 0; cycle < initializationCycles; ++cycle) {
                Require(object.Init(nullptr, requestedFeatures, 0) == S_OK && state.references == 2,
                    "reinitialization replaces one owned backend reference without accumulating QI leases");
            }
        }
        if (mode == "repeat-failure" && nativeWidth) {
            IA3dDal* priorBackend = object.m_pBackend;
            state.initializationResult = E_FAIL;
            constexpr int failureCycles = 64;
            for (int cycle = 0; cycle < failureCycles; ++cycle) {
                Require(object.Init(nullptr, requestedFeatures, 0) == A3DERROR_FAILED_INIT
                    && object.m_pBackend == priorBackend && state.references == 2,
                    "failed reinitialization releases temporary leases and retains the prior owned backend");
            }
        }
        if (mode == "compatibility") {
            Require(object.compatibilityOperation == A3D_COMPAT_LEGACY_MARKER
                && object.compatibilityValue == A3D_COMPAT_LEGACY_MARKER_VALUE,
                "unchanged caps request triggers existing compatibility operation");
            Require(!(object.m_dwFeaturesAvailable & A3D_1ST_REFLECTIONS), "existing compatibility reflection mask retained");
        }
    } else if (nativeWidth) {
        Require(state.references == 1 && object.m_pBackend == nullptr, "all failed native QI leases released");
    }
    object.ShutdownBackend();
    Require(state.references == 0 && state.destroys == 1, "valid shutdown releases transferred backend and DirectSound ownership");
    std::printf("PASS %s pointer=%zu native_calls=%d legacy_calls=%d default_calls=%d\n", mode.c_str(),
        sizeof(void*), state.nativeInitializations, state.legacyInitializations, state.defaultInitializations);
}

int main(int argc, char** argv) {
    Require(argc == 2, "one fixture case");
#if defined(_WIN32)
    if (std::strcmp(argv[1], "real-default") == 0) {
        const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        Require(SUCCEEDED(com), "initialize COM for read-only DirectSound initialization probe");
        IDirectSound* directSound = nullptr;
        const HRESULT created = CoCreateInstance(CLSID_DirectSound, nullptr, CLSCTX_INPROC_SERVER,
            IID_IDirectSound, reinterpret_cast<void**>(&directSound));
        std::printf("real DirectSound CoCreateInstance HRESULT=%08lx\n", static_cast<unsigned long>(created));
        Require(SUCCEEDED(created), "create actual uninitialized system DirectSound object");
        CA3d object;
        object.SetupBackend(directSound, nullptr, nullptr);
        const HRESULT initialized = object.Init(nullptr, 0, 0);
        std::printf("actual CA3d::Init(default) HRESULT=%08lx legacy_backend=%p\n",
            static_cast<unsigned long>(initialized), static_cast<void*>(object.m_pBackend));
        object.ShutdownBackend();
        CoUninitialize();
        Require(initialized == S_OK, "actual default DirectSound initialization; no buffer/play/output");
        return 0;
    }
#endif
    Test(argv[1]);
}
