#include "fixture.h"

int main() {
    static_assert(A3D_DIRECT_PATH_A3D == 0x08 && A3D_1ST_REFLECTIONS == 0x02);
    static_assert(A3D_DIRECT_PATH_GENERIC == 0x20 && A3D_OCCLUSIONS == 0x40);
    static_assert(A3D_DISABLE_SPLASHSCREEN == 0x80);
    constexpr DWORD kQualified = A3D_DIRECT_PATH_A3D | A3D_OCCLUSIONS;
    CA3d* initial = CreateParent();
    CHECK(initial->Init(nullptr, 0, 0) == S_OK);
    CHECK(initial->m_dwFeaturesAvailable == kQualified);
    CHECK(initial->Disable(A3D_OCCLUSIONS) == S_OK);
    CHECK(initial->Enable(A3D_OCCLUSIONS) == S_OK);
    CHECK(initial->IsFeatureAvailable(kQualified | A3D_1ST_REFLECTIONS) != S_OK);
    CHECK(initial->Enable(kQualified | A3D_1ST_REFLECTIONS) == A3DERROR_FEATURE_NOT_SUPPORTED);
    CHECK(initial->Release() == 0);
    for (bool leaf : {false, true}) {
        for (unsigned bit = 0; bit < 32; ++bit) {
            const DWORD feature = DWORD{1} << bit;
            if ((feature & kQualified) || feature == A3D_DISABLE_SPLASHSCREEN) {
                continue;
            }
            IUnknown* provider = nullptr;
            CHECK((leaf ? A3dCreateNativeLeaf(&provider) : A3dCreateNativeProvider(&provider)) == S_OK);
            IA3dNativeDeviceV1* device = nullptr;
            CHECK(provider->QueryInterface(IID_IA3dNativeDeviceV1, reinterpret_cast<void**>(&device)) == S_OK);
            DWORD supported = UINT32_MAX;
            CHECK(device->InitializeDevice(nullptr, feature | kQualified, 1, &supported) ==
                A3DERROR_FEATURE_NOT_SUPPORTED);
            CHECK(supported == 0);
            IDirectSound* sound = nullptr;
            CHECK(provider->QueryInterface(IID_IDirectSound, reinterpret_cast<void**>(&sound)) == S_OK);
            DSCAPS caps{};
            caps.dwSize = sizeof(caps);
            CHECK(sound->GetCaps(&caps) == DSERR_UNINITIALIZED);
            sound->Release();
            device->Release();
            provider->Release();
        }
        IUnknown* provider = nullptr;
        CHECK((leaf ? A3dCreateNativeLeaf(&provider) : A3dCreateNativeProvider(&provider)) == S_OK);
        IA3dNativeDeviceV1* device = nullptr;
        CHECK(provider->QueryInterface(IID_IA3dNativeDeviceV1, reinterpret_cast<void**>(&device)) == S_OK);
        DWORD supported = UINT32_MAX;
        CHECK(device->InitializeDevice(nullptr, kQualified | A3D_DISABLE_SPLASHSCREEN, 1, &supported) == S_OK);
        CHECK(supported == kQualified);
        device->Release();
        provider->Release();
    }
    for (unsigned bit = 0; bit < 32; ++bit) {
        const DWORD feature = DWORD{1} << bit;
        if ((feature & kQualified) || feature == A3D_DISABLE_SPLASHSCREEN) {
            continue;
        }
        CA3d* parent = CreateParent();
        CHECK(parent->Init(nullptr, feature | kQualified, 0) == A3DERROR_FEATURE_NOT_SUPPORTED);
        CHECK(parent->m_dwFeaturesAvailable == 0 && parent->m_pBackend == nullptr);
        CHECK(parent->m_dwRequestedFeatures == 0);
        CHECK(dynamic_cast<ResMan*>(parent->m_pDirectSound)->m_dwInitialized == 0);
        CHECK(parent->Init(nullptr, kQualified, 0) == S_OK);
        CHECK(parent->IsFeatureAvailable(kQualified) == S_OK);
        CHECK(parent->IsFeatureAvailable(kQualified | feature) != S_OK);
        const DWORD enabledBefore = parent->m_dwEnabledFeatures;
        CHECK(parent->Enable(kQualified | feature) == A3DERROR_FEATURE_NOT_SUPPORTED);
        CHECK(parent->m_dwEnabledFeatures == enabledBefore);
        CHECK(!parent->IsEnabled(feature));
        CHECK(parent->Release() == 0);
    }
    CA3d* parent = CreateParent();
    CHECK(parent->Init(nullptr, A3D_DISABLE_SPLASHSCREEN, 0) == S_OK);
    CHECK(parent->m_dwFeaturesAvailable == kQualified);
    CHECK(parent->IsFeatureAvailable(0) != S_OK);
    CHECK(parent->Enable(0) == A3DERROR_FEATURE_NOT_SUPPORTED);
    IA3dGeomTarget* geometry = nullptr;
    CHECK(parent->QueryInterface(IID_IA3dGeom, reinterpret_cast<void**>(&geometry)) == S_OK);
    CHECK(!parent->IsEnabled(A3D_DIRECT_PATH_GENERIC));
    CHECK(parent->Enable(kQualified) == S_OK);
    CHECK(parent->IsEnabled(kQualified));
    CHECK(parent->IsEnabled(A3D_OCCLUSIONS));
    CHECK(parent->Disable(kQualified | A3D_OCCLUSIONS) == S_OK);
    CHECK(parent->m_dwEnabledFeatures == 0);
    geometry->Release();
    CHECK(parent->Release() == 0);
    std::puts("PASS native_feature_negotiation");
}
