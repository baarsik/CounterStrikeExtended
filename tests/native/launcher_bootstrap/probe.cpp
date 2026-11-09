#undef main
#include "public/tier1/interface.h"
#include "../public/native_module.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

int CseActualLauncherMain(int, const char**, const char**);
CreateInterfaceFn Sys_GetFactory(const char*);

namespace {
constexpr int kMissingProbeArguments = 10;
constexpr int kAbsentModuleLoadedByQuery = 11;
constexpr int kOwnedModuleLoadFailed = 12;
constexpr int kOwnedFactoryMissing = 13;
constexpr int kMissingLauncherArguments = 14;
constexpr int kPeerReferenceInvalidated = 15;
constexpr int kReleasedModuleStillLoaded = 16;
constexpr int kNullFactoryModuleStillLoaded = 17;
}

int main(int argc, const char** argv) {
    if (argc < 3) {
        return kMissingProbeArguments;
    }
    if (std::strcmp(argv[1], "nullfs") == 0) {
        const int result = CseActualLauncherMain(argc - 3, argv + 3, nullptr);
        return cse_platform::FindLoadedModule(argv[2]) ? kNullFactoryModuleStillLoaded : result;
    }
    const bool retained = std::strcmp(argv[1], "retained") == 0;
    if (!retained) {
        if (Sys_GetFactory(argv[2]) != nullptr) {
            return kAbsentModuleLoadedByQuery;
        }
    }
    void* owner = cse_platform::LoadModule(argv[2], cse_platform::ModuleFallback::None);
    if (!owner) {
        return kOwnedModuleLoadFailed;
    }
    if (!Sys_GetFactory(argv[2])) {
        return kOwnedFactoryMissing;
    }
    int result = 0;
    if (retained) {
        if (argc < 4) {
            return kMissingLauncherArguments;
        }
        result = CseActualLauncherMain(argc - 3, argv + 3, nullptr);
        const auto alive = reinterpret_cast<int(*)()>(cse_platform::GetSymbol(owner, "FixtureEngineAlive"));
        if (!alive || alive() != 1) {
            return kPeerReferenceInvalidated;
        }
    }
    cse_platform::UnloadModule(owner);
    if (!retained && Sys_GetFactory(argv[2]) != nullptr) {
        return kReleasedModuleStillLoaded;
    }
    std::printf("probe_retained=%d actual_entry_result=%d\n", retained ? 1 : 0, result);
    return result;
}
