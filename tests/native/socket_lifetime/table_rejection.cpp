#include "SocketImageOwner.h"
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
using namespace cse::socket_lifetime;
extern "C" const HostTable CseTestHiddenSocketHost;
#if CSE_BAD_TABLE != 0
extern "C" __attribute__((visibility("default"))) const HostTable CseNativeSocketHostV1 = {
    CSE_BAD_TABLE == 1 ? 0u : sizeof(HostTable), CSE_BAD_TABLE == 2 ? 0u : Version,
    CSE_BAD_TABLE == 3 ? 0u : HostContract,
    CseTestHiddenSocketHost.owns, CseTestHiddenSocketHost.createUdp,
    CseTestHiddenSocketHost.closeUdp, CseTestHiddenSocketHost.quiescent};
#endif
static unsigned int effects = 0;
extern "C" void __cyg_profile_func_enter(void* function, void*) {
    if (function == reinterpret_cast<void*>(CseTestHiddenSocketHost.owns) ||
        function == reinterpret_cast<void*>(CseTestHiddenSocketHost.createUdp) ||
        function == reinterpret_cast<void*>(CseTestHiddenSocketHost.closeUdp) ||
        function == reinterpret_cast<void*>(CseTestHiddenSocketHost.quiescent)) {
        ++effects;
    }
}
extern "C" void __cyg_profile_func_exit(void*, void*) {}
static void Require(bool condition, const char* text) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", text);
        std::abort();
    }
}
int main(int argc, char** argv) {
    Require(argc == 3, "provider and shadow table image");
    Require(InitializeProcess(), "actual executable initialization");
    void* shadow = dlopen(argv[2], RTLD_NOW | RTLD_GLOBAL);
    Require(shadow && dlsym(shadow, HostSymbol), "same-name shadow table really loaded");
    void* image = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    Require(image != nullptr, "actual producer image");
    SocketImageOwner owner;
    effects = 0;
    Require(!owner.Open(image, argv[1]) && !owner.Active(), "missing or incompatible canonical main table rejected");
    Require(effects == 0, "no callback before whole table acceptance");
    Require(dlclose(image) == 0 && dlclose(shadow) == 0, "normal rejection cleanup");
    std::printf("canonical_table_negative=%d adapter_effects=%u\n", CSE_BAD_TABLE, effects);
}
