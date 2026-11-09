#include "pman_main.h"
#include "cvardef.h"
#include "native_module.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool condition, const char* description) {
    if (!condition) {
        std::fprintf(stderr, "Particle module failure: %s\n", description);
        std::exit(EXIT_FAILURE);
    }
}

cvar_t statistics = {};

cvar_t* RegisterVariable(char*, char*, int) {
    return &statistics;
}

void IgnoreWarning(char*, ...) {}

}

void RunModule(const char* path, const char* scenario) {
    void* module = cse_platform::LoadModule(path, cse_platform::ModuleFallback::None);
    Require(module != nullptr, "load actual manifest-owned particle module");
    const auto factory = reinterpret_cast<CreateInterfaceFn>(
        cse_platform::GetSymbol(module, cse_platform::InterfaceFactorySymbol));
    Require(factory != nullptr, "actual module exposes CreateInterface");
    int result = IFACE_FAILED;
    auto* manager = static_cast<IParticleMan*>(factory(PARTICLEMAN_INTERFACE, &result));
    Require(manager != nullptr && result == IFACE_OK, "create actual particle interface");
    cl_enginefunc_t engine = {};
    engine.pfnRegisterVariable = RegisterVariable;
    engine.Con_DPrintf = IgnoreWarning;
    manager->SetUp(&engine);
    manager->SetRender(0);
    Require(manager->RequestNewMemBlock(static_cast<int>(sizeof(CBaseParticle))) == nullptr,
        "disabled actual module does not allocate a block");
    if (std::strcmp(scenario, "unused") != 0) {
        manager->SetRender(1);
        Require(manager->RequestNewMemBlock(static_cast<int>(sizeof(CBaseParticle))) != nullptr,
            "actual module allocates native particle storage");
        if (std::strcmp(scenario, "reset") == 0) {
            manager->ResetParticles();
        }
    }
    // This pinned fixture knows the concrete factory type. The public interface
    // has no release method; delete through its real virtual concrete destructor.
    delete static_cast<IParticleMan_Active*>(manager);
    cse_platform::UnloadModule(module);
    Require(cse_platform::FindLoadedModule(path) == nullptr, "actual particle image unloads");
}

int main(int argc, char** argv) {
    Require(argc == 3, "actual particle module path and scenario are required");
    const char* scenario = argv[2];
    Require(std::strcmp(scenario, "reset") == 0 || std::strcmp(scenario, "active") == 0
        || std::strcmp(scenario, "unused") == 0 || std::strcmp(scenario, "reload") == 0, "known module scenario");
    constexpr int reloadCycles = 64;
    const int cycles = std::strcmp(scenario, "reload") == 0 ? reloadCycles : 1;
    for (int cycle = 0; cycle < cycles; ++cycle) {
        RunModule(argv[1], scenario);
    }
    std::printf("Particle module %s passed\n", scenario);
    return EXIT_SUCCESS;
}
