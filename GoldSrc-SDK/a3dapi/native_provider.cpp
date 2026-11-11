#include "native_provider.h"
#include "native_capacity.h"
#include "native_buffer.h"
#include "native_output.h"
#include "dal_a2d.h"
#include <new>
#include <cstring>

namespace {
constexpr DWORD kSoftwareVoices = 8; // Retained A2D pool limit; never hardware capacity.
constexpr DWORD kInitializationMode = 1;
// Qualified by the retained source -> complete control -> software PCM path.
// EQ/reflections remain stock-unread; generic/occlusion rendering is unqualified.
constexpr DWORD kQualifiedRenderingFeatures = A3D_DIRECT_PATH_A3D;

HRESULT ValidateRequest(DWORD features, DWORD mode, DWORD* supported) {
    if (!supported) {
        return E_POINTER;
    }
    *supported = 0;
    if (features & ~(kQualifiedRenderingFeatures | A3D_DISABLE_SPLASHSCREEN)) {
        return A3DERROR_FEATURE_NOT_SUPPORTED;
    }
    return mode == kInitializationMode ? S_OK : E_INVALIDARG;
}

class NativeLeaf final : public CA3dSoundBackend, public IA3dNativeDeviceV1, public IA3dNativeCapacityV1 {
public:
    NativeLeaf() {
        m_cReferences = 1;
    }
    ~NativeLeaf() {
        Shutdown();
    }
    void Shutdown() noexcept {
        std::lock_guard<std::recursive_mutex> admission(admission_);
        output_.Close();
        mixer_.Shutdown();
        initialized_ = false;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return A3dReferenceIncrement(&m_cReferences);
    }
    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG remaining = A3dReferenceDecrement(&m_cReferences);
        if (!remaining) {
            delete this;
        }
        return remaining;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override {
        if (!object) {
            return E_POINTER;
        }
        *object = nullptr;
        if (IsEqualIID(iid, IID_IA3dNativeDeviceV1)) {
            *object = static_cast<IA3dNativeDeviceV1*>(this);
        } else if (IsEqualIID(iid, IID_IA3dNativeCapacityV1)) {
            *object = static_cast<IA3dNativeCapacityV1*>(this);
        } else {
            return CA3dSoundBackend::QueryInterface(iid, object);
        }
        AddRef();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE InitializeDevice(const GUID* device, DWORD features, DWORD mode,
        DWORD* supported) override {
        std::lock_guard<std::recursive_mutex> admission(admission_);
        HRESULT result = ValidateRequest(features, mode, supported);
        if (FAILED(result)) {
            return result;
        }
        if (initialized_) {
            return E_INVALIDARG;
        }
        const GUID requested = device ? *device : GUID{};
        result = output_.Open(device ? &requested : nullptr, mixer_);
        if (SUCCEEDED(result) && !A3dNativeAcquisitionSucceeded()) {
            result = E_OUTOFMEMORY;
        }
        if (SUCCEEDED(result)) {
            result = output_.Start();
            if (SUCCEEDED(result) && !A3dNativeAcquisitionSucceeded()) {
                result = E_OUTOFMEMORY;
            }
        }
        if (FAILED(result)) {
            output_.Close();
            return result;
        }
        device_ = requested;
        initialized_ = true;
        *supported = kQualifiedRenderingFeatures;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Initialize(LPCGUID device) override {
        DWORD supported = 0;
        return InitializeDevice(device, 0, kInitializationMode, &supported);
    }
    HRESULT STDMETHODCALLTYPE GetCaps(LPDSCAPS caps) override {
        std::lock_guard<std::recursive_mutex> admission(admission_);
        if (!caps || caps->dwSize != sizeof(*caps)) {
            return E_INVALIDARG;
        }
        *caps = {};
        caps->dwSize = sizeof(*caps);
        caps->dwFlags = DSCAPS_PRIMARYSTEREO | DSCAPS_PRIMARY16BIT | DSCAPS_SECONDARYMONO |
            DSCAPS_SECONDARYSTEREO | DSCAPS_SECONDARY8BIT | DSCAPS_SECONDARY16BIT;
        return initialized_ ? S_OK : DSERR_UNINITIALIZED;
    }
    HRESULT STDMETHODCALLTYPE GetVoiceCapacity(DWORD* total, DWORD* available) override {
        std::lock_guard<std::recursive_mutex> admission(admission_);
        if (!total || !available) {
            return E_POINTER;
        }
        const DWORD used = mixer_.BufferCount();
        *total = kSoftwareVoices;
        *available = used < kSoftwareVoices ? kSoftwareVoices - used : 0;
        return initialized_ ? S_OK : DSERR_UNINITIALIZED;
    }
    HRESULT STDMETHODCALLTYPE CreateSoundBuffer(LPCDSBUFFERDESC desc, IDirectSoundBuffer** buffer,
        IUnknown* outer) override {
        std::lock_guard<std::recursive_mutex> admission(admission_);
        if (!buffer) {
            return E_POINTER;
        }
        *buffer = nullptr;
        if (outer) {
            return CLASS_E_NOAGGREGATION;
        }
        if (!initialized_) {
            return DSERR_UNINITIALIZED;
        }
        if (mixer_.BufferCount() >= kSoftwareVoices) {
            return E_OUTOFMEMORY;
        }
        HRESULT result = mixer_.CreateBuffer(desc, buffer);
        if (SUCCEEDED(result)) {
            static_cast<A3dNativeBuffer*>(*buffer)->RetainNativeOwner(static_cast<IDirectSound*>(this));
        }
        return result;
    }
    HRESULT STDMETHODCALLTYPE CreateSoundBufferEx(const a3d_platform_buffer_desc_t* desc, BYTE* mode,
        IDirectSoundBuffer** buffer, IUnknown* outer) override {
        if (mode) {
            *mode = 0;
        }
        if (!desc) {
            if (buffer) {
                *buffer = nullptr;
            }
            return E_POINTER;
        }
        DSBUFFERDESC copy{};
        copy.dwSize = sizeof(copy);
        copy.dwFlags = desc->dwFlags;
        copy.dwBufferBytes = desc->dwBufferBytes;
        copy.lpwfxFormat = desc->lpwfxFormat;
        return CreateSoundBuffer(&copy, buffer, outer);
    }
    HRESULT STDMETHODCALLTYPE DuplicateSoundBuffer(IDirectSoundBuffer* source, IDirectSoundBuffer** buffer) override {
        std::lock_guard<std::recursive_mutex> admission(admission_);
        if (!buffer) {
            return E_POINTER;
        }
        *buffer = nullptr;
        if (mixer_.BufferCount() >= kSoftwareVoices) {
            return E_OUTOFMEMORY;
        }
        HRESULT result = mixer_.DuplicateBuffer(source, buffer);
        if (SUCCEEDED(result)) {
            static_cast<A3dNativeBuffer*>(*buffer)->RetainNativeOwner(static_cast<IDirectSound*>(this));
        }
        return result;
    }
    HRESULT STDMETHODCALLTYPE GetDS(IDirectSound** sound) override {
        if (!sound) {
            return E_POINTER;
        }
        *sound = static_cast<IDirectSound*>(this);
        AddRef();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetResourceManagerMode(DWORD mode) override {
        return mode == A3D_RESOURCE_MODE_OFF ? S_OK : DSERR_UNSUPPORTED;
    }
private:
    // Destruction order is explicit: Close joins production and quiesces the
    // SDL callback before the mixer can retire any borrowed buffer entries.
    std::recursive_mutex admission_;
    A3dNativeOwnerLease lifetime_{A3dNativeOwnerKind::Leaf};
    A3dNativeMixer mixer_;
    A3dNativeOutput output_;
    GUID device_{};
    bool initialized_ = false;
};

class NativeProvider final : public ResMan, public IA3dNativeDeviceV1 {
    A3dNativeOwnerLease lifetime_{A3dNativeOwnerKind::Provider};
public:
    NativeProvider() {
        m_cReferences = 1;
    }
    ~NativeProvider() {
        ShutdownNative();
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return A3dReferenceIncrement(&m_cReferences);
    }
    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG remaining = A3dReferenceDecrement(&m_cReferences);
        if (!remaining) {
            delete this;
        }
        return remaining;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override {
        if (!object) {
            return E_POINTER;
        }
        if (IsEqualIID(iid, IID_IA3dNativeDeviceV1)) {
            *object = static_cast<IA3dNativeDeviceV1*>(this);
            AddRef();
            return S_OK;
        }
        return ResMan::QueryInterface(iid, object);
    }
    HRESULT STDMETHODCALLTYPE InitializeDevice(const GUID* device, DWORD features, DWORD mode,
        DWORD* supported) override {
        std::lock_guard<std::recursive_mutex> admission(m_nativeAdmission);
        HRESULT result = ValidateRequest(features, mode, supported);
        if (FAILED(result)) {
            return result;
        }
        if (m_dwInitialized) {
            return E_INVALIDARG;
        }
        IUnknown* leaf = nullptr;
        result = A3dCreateNativeLeaf(&leaf);
        if (FAILED(result)) {
            return result;
        }
        IA3dNativeDeviceV1* typed = nullptr;
        result = leaf->QueryInterface(IID_IA3dNativeDeviceV1, reinterpret_cast<void**>(&typed));
        if (SUCCEEDED(result) && !A3dNativeAcquisitionSucceeded()) {
            result = E_OUTOFMEMORY;
        }
        if (SUCCEEDED(result)) {
            result = typed->InitializeDevice(device, features, mode, supported);
        }
        if (typed) {
            typed->Release();
        }
        if (SUCCEEDED(result)) {
            result = AttachNativeBackend(leaf);
        }
        leaf->Release();
        if (FAILED(result)) {
            *supported = 0;
        }
        return result;
    }
};
}

HRESULT A3dCreateNativeLeaf(IUnknown** leaf) {
    if (!leaf) {
        return E_POINTER;
    }
    *leaf = nullptr;
    auto* result = new (std::nothrow) NativeLeaf;
    if (!result) {
        return E_OUTOFMEMORY;
    }
    if (!A3dNativeAcquisitionSucceeded()) {
        delete result;
        return E_OUTOFMEMORY;
    }
    *leaf = static_cast<IDirectSound*>(result);
    return S_OK;
}

HRESULT A3dCreateNativeProvider(IUnknown** provider) {
    if (!provider) {
        return E_POINTER;
    }
    *provider = nullptr;
    auto* result = new (std::nothrow) NativeProvider;
    if (!result) {
        return E_OUTOFMEMORY;
    }
    *provider = static_cast<IDirectSound*>(result);
    return S_OK;
}

bool A3dIsNativeProvider(IUnknown* object) noexcept {
    return dynamic_cast<NativeProvider*>(object) != nullptr;
}

void A3dShutdownNativeLeaf(IUnknown* object) noexcept {
    if (auto* leaf = dynamic_cast<NativeLeaf*>(object)) {
        leaf->Shutdown();
    }
}
