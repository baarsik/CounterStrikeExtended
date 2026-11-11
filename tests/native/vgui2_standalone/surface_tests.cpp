#include <cstdio>
#include <dlfcn.h>
#include "vgui/ISurface.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        return 2;
    }
    void* module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!module) {
        std::fprintf(stderr, "%s\n", dlerror());
        return 2;
    }
    auto factory = reinterpret_cast<CreateInterfaceFn>(dlsym(module, "CreateInterface"));
    auto* surface = factory ? static_cast<vgui2::ISurface*>(factory("VGUI_Surface026", nullptr)) : nullptr;
    if (!surface) {
        std::fprintf(stderr, "FAIL: actual owning VGUI library does not publish VGUI_Surface026\n");
        dlclose(module);
        return 1;
    }
    dlclose(module);
    return 0;
}
