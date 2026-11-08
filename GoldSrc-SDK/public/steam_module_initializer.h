#ifndef CSE_STEAM_MODULE_INITIALIZER_H
#define CSE_STEAM_MODULE_INITIALIZER_H

#if defined(CSE_OWNED_STEAM_PROVIDER)
#if defined(_WIN32)
extern "C" void __cdecl CseSteamModuleInitialize();
#else
extern "C" __attribute__((visibility("hidden"))) void CseSteamModuleInitialize();
#endif

namespace {
// Forced before every consumer translation unit: even indirect SDK users can call during global teardown.
// Constructing the provider here registers its destructor before all consumer globals.
struct CseSteamProviderInitializer {
    CseSteamProviderInitializer() {
        CseSteamModuleInitialize();
    }
};
CseSteamProviderInitializer cseSteamProviderInitializer;
} // namespace
#endif
#endif
