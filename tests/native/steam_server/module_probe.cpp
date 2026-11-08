#include "native_module.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace {
void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "server_module_fixture_failure=%s\n", message);
        std::exit(1);
    }
}
}
int main(int argc, char** argv) {
    Require(argc == 3 || argc == 4, "owned module path, app10/app70 and optional context mode");
    const char* mode = argc == 4 ? argv[3] : nullptr;
    constexpr int ModuleCycles = 3;
    for (int cycle = 0; cycle < ModuleCycles; ++cycle) {
        Require(!cse_platform::FindLoadedModule(argv[1]), "server consumer starts unloaded");
        void* module = cse_platform::LoadModule(argv[1], cse_platform::ModuleFallback::None);
        Require(module != nullptr, "owned server consumer load");
        using Run = int (*)(const char*, const char*);
        const auto run = reinterpret_cast<Run>(cse_platform::GetSymbol(module, "CseSteamServerFixtureRun"));
        Require(run != nullptr, "owned server fixture entry");
        Require(run(argv[2], mode) == 0, "actual SDK server sessions");
        cse_platform::UnloadModule(module);
        Require(!cse_platform::FindLoadedModule(argv[1]), "owned server consumer fully unloads");
        std::printf("owned_server_module_cycle=%d passed=1\n", cycle + 1);
        std::fflush(stdout);
    }
    std::puts("native_steam_server_module_passed=1");
}
