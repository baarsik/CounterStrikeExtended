#include "launcher/native/SoftwareImageOwner.h"
#include "public/native_vgui_completion.h"
#include <cassert>
#include <cstdio>
#include <dlfcn.h>
int main(int argc, char** argv) {
    assert(argc == 2);
    void* module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    assert(module);
    const auto calls = reinterpret_cast<unsigned int (*)()>(dlsym(module, "BindCalls"));
    assert(calls && calls() == 0);
    cse::software_lifetime::ImageOwner owner;
    assert(!owner.Open(module, argv[1], cse::software_lifetime::Mode::Dedicated));
    assert(!owner.Active() && calls() == 0);
    assert(!cse::vgui_completion::Admit(module, dlsym(module, "CreateInterface")));
    assert(dlclose(module) == 0);
    std::puts("PASS private descriptor mismatch rejected before foreign bind");
}
