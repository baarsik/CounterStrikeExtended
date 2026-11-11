#include "MacRuntimeSession.h"
#include <cassert>
#include <cstdio>
#include <dlfcn.h>

int main(int argc, char** argv) {
    assert(argc == 4);
    void* foreign = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!foreign) {
        std::puts(dlerror());
        return 1;
    }
    const bool admitted = cse_mac_runtime::Session::Admit(argv[2], argv[3],
        cse_mac_runtime::EngineImage::Hardware);
    std::printf("admitted=%d\n", admitted);
    if (!admitted) {
        std::puts("constructor_not_entered");
        assert(dlclose(foreign) == 0);
        return 0;
    }
    // Diagnostic RED path: demonstrate which provider dyld actually reuses in
    // an engine constructor when the old basename-only gate admits this cohort.
    void* engine = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
    if (!engine) {
        std::puts(dlerror());
        return 2;
    }
    auto marker = reinterpret_cast<int (*)()>(dlsym(engine, "EngineMarker"));
    assert(marker);
    std::printf("constructor_provider=%d (owned=1 foreign=2)\n", marker());
    assert(dlclose(engine) == 0);
    assert(dlclose(foreign) == 0);
    return 1;
}
