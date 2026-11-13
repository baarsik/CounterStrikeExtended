#include "../../public/tier1/interface.h"
#include "Proxy.h"

#if !CSE_NATIVE_SYSTEM_MODULE
#ifdef _WIN32
EXPOSE_SINGLE_INTERFACE( Proxy, ISystemModule, "proxy001" );
#else
EXPOSE_SINGLE_INTERFACE( Proxy, IProxy, "proxy001" );
#endif

#endif

#if CSE_NATIVE_SYSTEM_MODULE
static Proxy nativeProxy;
static void* CreateNativeProxyView() {
    return static_cast<IProxy*>(&nativeProxy);
}
static InterfaceReg nativeProxyRegistration(CreateNativeProxyView, cse::modules::ProxyName);
#endif

#if CSE_NATIVE_SYSTEM_MODULE
static ISystemModule* CSE_ABI_CALL CreateNativeModule(const char* name) {
    return cse::modules::Is(name, cse::modules::ProxyName) ? static_cast<ISystemModule*>(&nativeProxy) : nullptr;
}

static void CSE_ABI_CALL ReleaseNativeModule(ISystemModule*) {
    // Provider-owned singleton: it remains alive until this image is unloaded.
}

static void* NativeFactory() {
    static cse::modules::Factory factory{cse::modules::Contract, sizeof(cse::modules::Factory),
        cse::modules::ContractRevision, CreateNativeModule, ReleaseNativeModule};
    return &factory;
}

static InterfaceReg nativeFactoryRegistration(NativeFactory, cse::modules::FactoryName);
#endif
