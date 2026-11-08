#include "native_module.h"

#include <cstdio>
#include <cstdlib>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Owned Steam module fixture failed: %s\n", message);
        std::exit(1);
    }
}
} // namespace

int main(int argc, char** argv) {
    Require(argc == 3, "two owned module paths required");
    constexpr int ModuleCycles = 3;
    for (int cycle = 0; cycle < ModuleCycles; ++cycle) {
        Require(!cse_platform::FindLoadedModule(argv[1]), "consumer starts unloaded");
        void* module = cse_platform::LoadModule(argv[1], cse_platform::ModuleFallback::None);
        Require(module != nullptr, "real provider binding during consumer static construction");
        using Run = int (*)(const char*);
        const auto run = reinterpret_cast<Run>(cse_platform::GetSymbol(module, "CseSteamServiceFixtureRun"));
        Require(run != nullptr, "owned module fixture entry point");
        void* peer = cse_platform::LoadModule(argv[2], cse_platform::ModuleFallback::None);
        Require(peer != nullptr, "second consumer binds the same real provider");
        const auto runPeer = reinterpret_cast<Run>(cse_platform::GetSymbol(peer, "CseSteamServiceFixtureRun"));
        Require(runPeer != nullptr, "second consumer entry point");
        Require(run(argv[0]) == 0 && runPeer(argv[0]) == 0, "both consumers use the actual SDK");
        cse_platform::UnloadModule(module);
        Require(!cse_platform::FindLoadedModule(argv[1]), "consumer releases its final owning load while peer remains");
        Require(runPeer(argv[0]) == 0, "remaining consumer works after the first unloads");
        cse_platform::UnloadModule(peer);
        Require(!cse_platform::FindLoadedModule(argv[2]), "second consumer releases its final owning load");
        std::printf("owned_module_cycle=%d passed=1\n", cycle + 1);
    }
    return 0;
}
