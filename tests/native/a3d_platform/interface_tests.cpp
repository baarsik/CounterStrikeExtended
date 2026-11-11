#include "platform_dsound.h"
#include "native_device_init.h"
#include "native_geometry_identity.h"
#include "A3d3.h"
#include "A3dSource.h"
#include "resman.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>

void Require(bool condition, const char* message = "A3D declaration contract failed") {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

// A marker return identifies the method actually reached through each slot.
// This fixture tests declaration ABI only; it does not fake production services.
constexpr HRESULT kSlotMarker = 100;
#define IMPLEMENT_SLOT(index, result, name, args, raw, invoke) \
    result STDMETHODCALLTYPE name args override { \
        return static_cast<result>(kSlotMarker + index); \
    }
#define CALLER_SLOT(index, result, name, args, raw, invoke) result (STDMETHODCALLTYPE* name) raw;
#define CHECK_SLOT(index, result, name, args, raw, invoke) \
    Require(table.name invoke == static_cast<result>(kSlotMarker + index), #name " slot mismatch");
#define DECLARE_PROBE(name) \
    static_assert(!std::has_virtual_destructor_v<name>); \
    struct name##Probe final : public name { SLOTS_##name(IMPLEMENT_SLOT) }; \
    struct name##Caller { SLOTS_##name(CALLER_SLOT) }; \
    void Check##name() { \
        name##Probe instance; \
        void* object = static_cast<name*>(&instance); \
        const void* address = nullptr; \
        std::memcpy(&address, object, sizeof(address)); \
        name##Caller table; \
        std::memcpy(&table, address, sizeof(table)); \
        SLOTS_##name(CHECK_SLOT) \
    }
#include "slot_contract.h"
#undef DECLARE_PROBE
#undef CHECK_SLOT
#undef CALLER_SLOT
#undef IMPLEMENT_SLOT

// This deliberately has two IUnknown subobjects so the native initialization
// view must adjust 'this', while its IUnknown query returns the canonical one.
constexpr DWORD kRequestedDirectPath = 0x08;
constexpr DWORD kProbeInitializationMode = 7;

class IdentityFixture final : public IClassFactory, public IA3dNativeDeviceV1 {
public:
    GUID received{};
    const GUID* receivedAddress = nullptr;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** output) override {
        if (!output) {
            return E_POINTER;
        }
        *output = nullptr;
        if (iid == IID_IUnknown || iid == IID_IClassFactory) {
            *output = static_cast<IClassFactory*>(this);
        } else if (iid == IID_IA3dNativeDeviceV1) {
            *output = static_cast<IA3dNativeDeviceV1*>(this);
        } else {
            return E_NOINTERFACE;
        }
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return ++references;
    }
    ULONG STDMETHODCALLTYPE Release() override {
        return --references;
    }
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown*, REFIID, void**) override {
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE LockServer(BOOL) override {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE InitializeDevice(const GUID* device, DWORD features, DWORD mode, DWORD* supported) override {
        Require(features == kRequestedDirectPath && mode == kProbeInitializationMode && supported != nullptr && device != nullptr);
        receivedAddress = device;
        received = *device;
        *supported = features;
        return S_OK;
    }
private:
    ULONG references = 1;
};

void CheckNativeIdentity() {
    IdentityFixture instance;
    IA3dNativeDeviceV1* native = nullptr;
    Require(instance.QueryInterface(IID_IA3dNativeDeviceV1, reinterpret_cast<void**>(&native)) == S_OK);
    IUnknown* canonical = nullptr;
    Require(native->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&canonical)) == S_OK);
    Require(canonical == static_cast<IClassFactory*>(&instance));
    // Heap addresses on the supported 64-bit native platforms exercise the
    // borrowed pointer lane with real readable GUID storage above UINT32_MAX.
    GUID* device = new GUID{0xfedcba98, 0x7654, 0x3210, {0x80, 1, 2, 3, 4, 5, 6, 7}};
#if UINTPTR_MAX > UINT32_MAX
    Require(reinterpret_cast<std::uintptr_t>(device) > UINT32_MAX);
#endif
    DWORD supported = 0;
    Require(native->InitializeDevice(device, kRequestedDirectPath, kProbeInitializationMode, &supported) == S_OK);
    Require(instance.receivedAddress == device && instance.received == *device && supported == kRequestedDirectPath);
    delete device;
    Require(canonical->Release() == 2);
    Require(native->Release() == 1);

#if UINTPTR_MAX > UINT32_MAX
    constexpr std::uintptr_t kLowPolygon = 0x12345678;
    constexpr std::uintptr_t kHighPolygon = 0x112345678;
    const auto low = A3dGeometryIdentity::OwnedPolygon(reinterpret_cast<void*>(kLowPolygon));
    const auto high = A3dGeometryIdentity::OwnedPolygon(reinterpret_cast<void*>(kHighPolygon));
    const auto tag = A3dGeometryIdentity::PublicTag(static_cast<DWORD>(kLowPolygon));
    Require(!(low == high) && !(tag == low) && tag == A3dGeometryIdentity::PublicTag(0x12345678));
#endif
}

void CheckInterfaceIds() {
    const GUID ids[] = {
        IID_IUnknown, IID_IClassFactory, IID_IDirectSound, IID_IDirectSoundBuffer,
        IID_IDirectSound3DBuffer, IID_IDirectSound3DListener, IID_IDirectSoundNotify,
        IID_IA3d, IID_IA3d2, IID_IA3d3, IID_IA3d4, IID_IA3dGeom, IID_IA3dSource,
        IID_IA3dListener, IID_IA3dList, IID_IA3dMaterial, IID_IA3dNativeDeviceV1
    };
    for (std::size_t first = 0; first < sizeof(ids) / sizeof(ids[0]); ++first) {
        for (std::size_t second = first + 1; second < sizeof(ids) / sizeof(ids[0]); ++second) {
            Require(!IsEqualGUID(ids[first], ids[second]));
        }
    }
    constexpr GUID expectedNative = {
        0x5951edd2, 0xe13f, 0x506b, {0xb3, 0x4e, 0xe0, 0x33, 0x6d, 0x86, 0x4d, 0xf8}
    };
    Require(IID_IA3dNativeDeviceV1 == expectedNative);
    Require(FAILED(E_NOINTERFACE) && FAILED(E_POINTER) && SUCCEEDED(S_FALSE));
}

int main() {
    CheckIUnknown();
    CheckIClassFactory();
    CheckIDirectSound();
    CheckIDirectSoundBuffer();
    CheckIDirectSound3DListener();
    CheckIDirectSound3DBuffer();
    CheckIDirectSoundNotify();
    CheckIA3d();
    CheckIA3d2();
    CheckIA3d3();
    CheckIA3d4();
    CheckIA3dGeom();
    CheckIA3dSource();
    CheckIA3dListener();
    CheckIA3dList();
    CheckIA3dMaterial();
    CheckIA3dGeomTarget();
    CheckIA3dListenerTarget();
    CheckIA3dSceneFactoryTarget();
    CheckIA3dCompatibilityTarget();
    CheckIA3dSource202();
    CheckIA3dSourcePrivate();
    CheckIA3dSourceRenderBuffer();
    CheckIA3dOpeningTarget();
    CheckIA3dDalBuffer();
    CheckIResManBuffer();
    CheckIA3dDal();
    CheckIA3dNativeDeviceV1();
    CheckNativeIdentity();
    CheckInterfaceIds();
    std::puts("complete_interface_views passed");
    return 0;
}
