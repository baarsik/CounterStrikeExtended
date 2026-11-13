#include "VGui1NativeRuntime.h"
#include "native_module.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {
void Require(bool value) {
    if (!value) {
        std::fputs("FAILED actual UTF8 atlas host\n", stderr);
        std::exit(1);
    }
}
bool Defer(void* context, void* reference) noexcept {
    static_cast<std::vector<void*>*>(context)->push_back(reference);
    return true;
}
template<typename Function>
Function Symbol(void* module, const char* name) {
    auto function = reinterpret_cast<Function>(cse_platform::GetSymbol(module, name));
    Require(function != nullptr);
    return function;
}
}

int main(int argc, char** argv) {
    Require(argc == 3);
    void* module = cse_platform::LoadModule(argv[1], cse_platform::ModuleFallback::None);
    Require(module != nullptr);
    std::vector<void*> deferred;
    const cse_vgui1_native::HostReleasesV1 host{sizeof(host), cse_vgui1_native::HostReleasesVersion, &deferred, Defer};
    using Open = int (*)(std::uint64_t, const char*, const char*, const cse_vgui1_native::HostReleasesV1*);
    using Call = int (*)();
    Require(Symbol<Open>(module, "FixtureOpen")(1, argv[1], argv[2], &host) == 0);
    Require(Symbol<Call>(module, "FixtureUtf8Suite")() == 0);
    Require(Symbol<Call>(module, "FixtureClose")() == 0);
    for (void* reference : deferred) {
        cse_platform::UnloadModule(reference);
    }
    cse_platform::UnloadModule(module);
}
