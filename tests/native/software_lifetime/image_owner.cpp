#include "launcher/native/SoftwareImageOwner.h"
#include <cassert>
#include <cstdio>
#include <dlfcn.h>
#include <thread>
#include <mach-o/dyld.h>

using namespace cse::software_lifetime;
int main(int argc, char** argv) {
    assert(argc == 2);
    for (int cycle = 0; cycle < 3; ++cycle) {
        void* initial = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
        assert(initial);
        const auto ready = reinterpret_cast<void (*)(bool)>(dlsym(initial, "SetResourcesReady"));
        const auto enter = reinterpret_cast<bool (*)()>(dlsym(initial, "EnterClient"));
        assert(ready && enter);
        ImageOwner owner;
        assert(owner.Open(initial, argv[1], Mode::Client));
        assert(dlclose(initial) == 0); // The actual helper's independent anchor remains the real owner.
        assert(owner.Active());
        {
            ImageOwner::Call call(&owner);
            assert(call && !owner.Retire());
        }
        std::thread foreign([&]() { assert(!owner.Retire()); });
        foreign.join();
        assert(!owner.Retire() && owner.Active());
        assert(!enter() && !owner.Enter());
        ready(true);
        void* inspect = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL | RTLD_NOLOAD);
        assert(inspect);
        const void* api = dlsym(inspect, Symbol);
        assert(api && dlclose(inspect) == 0);
        Dl_info mapped{};
        assert(dladdr(api, &mapped));
        const void* base = mapped.dli_fbase;
        assert(owner.Retire() && !owner.Active() && owner.Retire());
        for (std::uint32_t index = 0; index < _dyld_image_count(); ++index) {
            assert(_dyld_get_image_header(index) != base);
        }
    }
    std::puts("PASS actual executable owner anchor/call rejection, retained failed close, retry and physical unmap");
}
