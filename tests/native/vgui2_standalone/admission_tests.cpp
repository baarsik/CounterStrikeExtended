#include <SDL3/SDL.h>
#include "FileSystem.h"
#include "native_standalone_abi.h"
#include "native_filesystem_module.h"
#include "native_standalone_cohort.h"
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>

namespace {
void Require(bool valid, const char* message) {
    if (!valid) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
} // namespace

int main(int argc, char** argv) {
    Require(argc == 7, "expected VGUI/filesystem/hash/hash/wrong-role-image");
    constexpr char WrongHash[] = "0000000000000000000000000000000000000000000000000000000000000000";
    Require(!cse::standalone_cohort::Admit(argv[1], argv[2], WrongHash, argv[4]), "wrong image rejected before load");
    Require(!cse::standalone_cohort::AlreadyMapped(), "mismatch executes no module constructor");
    for (int index = 5; index < argc; ++index) {
        void* bad = dlopen(argv[index], RTLD_NOW | RTLD_LOCAL);
        Require(bad != nullptr, "load controlled incompatible image");
        auto count = reinterpret_cast<int (*)()>(dlsym(bad, "FactoryCalls"));
        Require(count && !cse::standalone_abi::CheckedFactory(bad) && count() == 0,
                "wrong role or physical contract rejects before its factory callback");
        dlclose(bad);
    }
    Require(cse::standalone_cohort::Admit(argv[1], argv[2], argv[3], argv[4]), "admit actual finished image cohort");
    void* filesystem = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
    void* vgui = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    Require(filesystem && vgui, "load actual admitted owners");
    auto factory = cse::standalone_abi::CheckedFactory(vgui);
    auto filesystemFactory = cse::filesystem_abi::CheckedFactory(filesystem);
    Require(factory && filesystemFactory && cse::standalone_abi::ValidateViews(factory, filesystemFactory),
            "all nine real views and all physical method addresses admitted");
    Require(SDL_WasInit(SDL_INIT_VIDEO) == 0 && SDL_WasInit(SDL_INIT_AUDIO) == 0,
            "factory discovery starts no video/audio subsystem");
    Require(!cse::standalone_cohort::Admit(argv[1], argv[2], argv[3], argv[4]),
            "disk hashes cannot attest an already-mapped image");
    dlclose(vgui);
    dlclose(filesystem);
    std::puts("PASS image/role/complete-view admission before callbacks and lazy video/audio");
}
