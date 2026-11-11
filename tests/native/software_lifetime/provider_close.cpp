#include <dlfcn.h>
#include <cassert>
#include <cstdio>
static bool failClose = true;
static int ControlledClose(void* module) {
    return failClose ? -1 : dlclose(module);
}
#define dlclose ControlledClose
#include "public/steam_provider.h"
#undef dlclose
int main(int argc, char** argv) {
    assert(argc == 2);
    cse_steam::Provider owner;
    assert(owner.Open(argv[1]));
    void* module = owner.NativeModule();
    const auto initialize = owner.Functions().initialize;
    assert(module && initialize);
#ifdef CSE_UNCHECKED_PROVIDER
    owner.Close();
#else
    assert(!owner.CloseChecked());
#endif
    assert(owner.NativeModule() == module && owner.Functions().initialize == initialize);
    failClose = false;
#ifndef CSE_UNCHECKED_PROVIDER
    assert(owner.CloseChecked() && !owner.NativeModule() && !owner.Functions().initialize);
    assert(owner.CloseChecked());
#endif
    std::puts(
        "PASS actual SDK provider retains complete binding on failed dlclose and clears only after retry success");
}
