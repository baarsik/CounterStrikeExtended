#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include "ia3dapi.h"
#include "factory_ids.inc"

#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "A3D factory probe failure: %s\n", message);
        std::exit(EXIT_FAILURE);
    }
}

int main(int argc, char** argv) {
    Require(argc == 2, "actual owned module path");
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Require(SUCCEEDED(com), "COM initialized for factory/provider presence probe");
    IUnknown* external = nullptr;
    const HRESULT oem = CoCreateInstance(s_A3dDalClassId, nullptr, CLSCTX_INPROC_SERVER,
        s_A3dDalInterfaceId, reinterpret_cast<void**>(&external));
    if (external) {
        external->Release();
    }
    IDirectSound* legacy = nullptr;
    const HRESULT oldA3d = CoCreateInstance(CLSID_A3d, nullptr, CLSCTX_INPROC_SERVER,
        IID_IDirectSound, reinterpret_cast<void**>(&legacy));
    if (legacy) {
        legacy->Release();
    }
    const auto module = LoadLibraryA(argv[1]);
    Require(module != nullptr, "load full actual A3D module");
    using GetFactory = HRESULT (__stdcall*)(REFCLSID, REFIID, void**);
    const auto factoryAddress = GetProcAddress(module, "DllGetClassObject");
    GetFactory getFactory = nullptr;
    static_assert(sizeof(getFactory) == sizeof(factoryAddress), "native exported function-pointer width");
    std::memcpy(&getFactory, &factoryAddress, sizeof(getFactory));
    Require(getFactory != nullptr, "retained exported class factory entry");
    IClassFactory* factory = nullptr;
    const HRESULT factoryResult = getFactory(CLSID_A3dApi, IID_IClassFactory, reinterpret_cast<void**>(&factory));
    Require(SUCCEEDED(factoryResult) && factory, "actual DllGetClassObject creates owned class factory");
    IUnknown* api = nullptr;
    const HRESULT created = factory->CreateInstance(nullptr, IID_IA3d3, reinterpret_cast<void**>(&api));
    if (api) {
        api->Release();
    }
    factory->Release();
    Require(FreeLibrary(module) != 0, "unload actual owned A3D module after factory failure");
    CoUninitialize();
    std::printf("pointer=%zu OEM_CLSID_442d12a1=%08lx legacy_CLSID_A3d=%08lx "
        "DllGetClassObject=%08lx CreateInstance=%08lx\n",
        sizeof(void*), static_cast<unsigned long>(oem), static_cast<unsigned long>(oldA3d),
        static_cast<unsigned long>(factoryResult), static_cast<unsigned long>(created));
    Require(oem == REGDB_E_CLASSNOTREG && oldA3d == REGDB_E_CLASSNOTREG && created == oldA3d,
        "measure original factory's missing OEM/legacy registrations without substituting a provider");
}
