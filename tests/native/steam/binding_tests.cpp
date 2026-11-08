#include "steam_provider.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
#define FIXTURE_EXPORT __declspec(dllexport)
#else
#define FIXTURE_EXPORT __attribute__((visibility("default")))
#endif

// A global symbol must never satisfy a failed explicit provider lookup.
extern "C" FIXTURE_EXPORT bool SteamAPI_Init() {
    std::abort();
}

namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Steam binding fixture failed: %s\n", message);
        std::exit(1);
    }
}
} // namespace

int main(int argc, char** argv) {
    Require(argc == 2, "explicit incomplete provider path required");
    cse_steam::Api api{};
    api.initialize = SteamAPI_Init;
    const char* missing = nullptr;
    Require(!cse_steam::BindApi(nullptr, api, missing), "null provider rejected even with a main-executable export");
    Require(!api.initialize && missing && std::strcmp(missing, "SteamAPI_Init") == 0,
        "failed binding clears the public table and reports its first missing symbol");
    cse_steam::Provider provider;
    Require(!provider.Open("cse-never-present-steam-provider"), "missing provider fails closed");
    Require(!provider.Functions().initialize && provider.LastError(), "missing provider exposes no callable table");
    Require(!provider.Open(argv[1]), "incomplete provider fails closed");
    Require(!provider.Functions().initialize, "partial native exports never escape the binding boundary");
    Require(std::strcmp(provider.LastError(), "SteamAPI_Shutdown") == 0, "first missing required export identified");
    Require(!cse_platform::FindLoadedModule(argv[1]), "failed provider open releases its owning module reference");
    provider.Close();
    provider.Close();
    Require(!provider.Functions().initialize, "repeated close leaves an empty function table");
    std::puts("Steam binding failures reject null, absent and partial providers atomically");
    return 0;
}
