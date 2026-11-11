#include "native_module.h"
#include <cassert>
#include <cstdio>
int main(int argc, char **argv) {
    assert(argc == 3);
    for (int cycle = 0; cycle < 3; ++cycle) {
        assert(!cse_platform::FindLoadedModule(argv[1]));
        void *module = cse_platform::LoadModule(argv[1], cse_platform::ModuleFallback::None);
        assert(module);
        auto run = reinterpret_cast<int (*)(const char *)>(cse_platform::GetSymbol(module, "CseSteamShutdownFixture"));
        assert(run && run(argv[2]) == 0);
#if defined(__APPLE__)
        auto retire = reinterpret_cast<bool (*)()>(cse_platform::GetSymbol(module, "CseSteamShutdownFixtureRetire"));
        assert(retire && retire() && retire());
#endif
        cse_platform::UnloadModule(module);
        assert(!cse_platform::FindLoadedModule(argv[1]));
        std::printf("shutdown_module_cycle=%d passed=1\n", cycle + 1);
    }
}
